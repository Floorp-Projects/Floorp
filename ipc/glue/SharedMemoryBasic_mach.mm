/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <map>

#include <mach/vm_map.h>
#include <mach/mach_port.h>
#if defined(XP_IOS)
#include <mach/vm_map.h>
#define mach_vm_address_t vm_address_t
#define mach_vm_allocate vm_allocate
#define mach_vm_deallocate vm_deallocate
#define mach_vm_map vm_map
#define mach_vm_read vm_read
#define mach_vm_region_recurse vm_region_recurse_64
#define mach_vm_size_t vm_size_t
#else
#include <mach/mach_vm.h>
#endif
#include <pthread.h>
#include <unistd.h>
#include "SharedMemoryBasic.h"
#include "chrome/common/mach_ipc_mac.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Printf.h"
#include "mozilla/StaticMutex.h"

#ifdef DEBUG
#define LOG_ERROR(str, args...)                 \
  PR_BEGIN_MACRO                                \
  char *msg = mozilla::Smprintf(str, ## args);  \
  NS_WARNING(msg);                              \
  mozilla::SmprintfFree(msg);                   \
  PR_END_MACRO
#else
#define LOG_ERROR(str, args...) do { /* nothing */ } while(0)
#endif

#define CHECK_MACH_ERROR(kr, msg)                               \
  PR_BEGIN_MACRO                                                \
  if (kr != KERN_SUCCESS) {                                     \
    LOG_ERROR("%s %s (%x)\n", msg, mach_error_string(kr), kr);  \
    return false;                                               \
  }                                                             \
  PR_END_MACRO

/*
 * This code is responsible for sharing memory between processes. Memory can be
 * shared between parent and child or between two children. Each memory region is
 * referenced via a Mach port. Mach ports are also used for messaging when
 * sharing a memory region.
 *
 * When the parent starts a child, it starts a thread whose only purpose is to
 * communicate with the child about shared memory. Once the child has started,
 * it starts a similar thread for communicating with the parent. Each side can
 * communicate with the thread on the other side via Mach ports. When either
 * side wants to share memory with the other, it sends a Mach message to the
 * other side. Attached to the message is the port that references the shared
 * memory region. When the other side receives the message, it automatically
 * gets access to the region. It sends a reply (also via a Mach port) so that
 * the originating side can continue.
 *
 * The two sides communicate using four ports. Two ports are used when the
 * parent shares memory with the child. The other two are used when the child
 * shares memory with the parent. One of these two ports is used for sending the
 * "share" message and the other is used for the reply.
 *
 * If a child wants to share memory with another child, it sends a "GetPorts"
 * message to the parent. The parent forwards this GetPorts message to the
 * target child. The message includes some ports so that the children can talk
 * directly. Both children start up a thread to communicate with the other child,
 * similar to the way parent and child communicate. In the future, when these
 * two children want to communicate, they re-use the channels that were created.
 *
 * When a child shuts down, the parent notifies all other children. Those
 * children then have the opportunity to shut down any threads they might have
 * been using to communicate directly with that child.
 */

namespace mozilla {
namespace ipc {

struct MemoryPorts {
  MachPortSender* mSender;
  ReceivePort* mReceiver;

  MemoryPorts() = default;
  MemoryPorts(MachPortSender* sender, ReceivePort* receiver)
   : mSender(sender), mReceiver(receiver) {}
};

// Protects gMemoryCommPorts and gThreads.
static StaticMutex gMutex;

static std::map<pid_t, MemoryPorts> gMemoryCommPorts;

enum {
  kGetPortsMsg = 1,
  kSharePortsMsg,
  kReturnIdMsg,
  kReturnPortsMsg,
  kShutdownMsg,
  kCleanupMsg,
};

const int kTimeout = 1000;
const int kLongTimeout = 60 * kTimeout;

pid_t gParentPid = 0;

struct PIDPair {
  pid_t mRequester;
  pid_t mRequested;

  PIDPair(pid_t requester, pid_t requested)
   : mRequester(requester), mRequested(requested) {}
};

struct ListeningThread {
  pthread_t mThread;
  MemoryPorts* mPorts;

  ListeningThread() = default;
  ListeningThread(pthread_t thread, MemoryPorts* ports)
   : mThread(thread), mPorts(ports) {}
};

struct SharePortsReply {
  uint64_t serial;
  mach_port_t port;
};

std::map<pid_t, ListeningThread> gThreads;

static void *
PortServerThread(void *argument);


static void
SetupMachMemory(pid_t pid,
                ReceivePort* listen_port,
                MachPortSender* listen_port_ack,
                MachPortSender* send_port,
                ReceivePort* send_port_ack,
                bool pidIsParent)
{
  if (pidIsParent) {
    gParentPid = pid;
  }
  auto* listen_ports = new MemoryPorts(listen_port_ack, listen_port);
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int err = pthread_create(&thread, &attr, PortServerThread, listen_ports);
  if (err) {
    LOG_ERROR("pthread_create failed with %x\n", err);
    return;
  }

  gMutex.AssertCurrentThreadOwns();
  gThreads[pid] = ListeningThread(thread, listen_ports);
  gMemoryCommPorts[pid] = MemoryPorts(send_port, send_port_ack);
}

// Send two communication ports to another process along with the pid of the process that is
// listening on them.
bool
SendPortsMessage(MachPortSender* sender,
                 mach_port_t ports_in_receiver,
                 mach_port_t ports_out_receiver,
                 PIDPair pid_pair)
{
  MachSendMessage getPortsMsg(kGetPortsMsg);
  if (!getPortsMsg.AddDescriptor(MachMsgPortDescriptor(ports_in_receiver))) {
    LOG_ERROR("Adding descriptor to message failed");
    return false;
  }
  if (!getPortsMsg.AddDescriptor(MachMsgPortDescriptor(ports_out_receiver))) {
    LOG_ERROR("Adding descriptor to message failed");
    return false;
  }

  getPortsMsg.SetData(&pid_pair, sizeof(PIDPair));
  kern_return_t err = sender->SendMessage(getPortsMsg, kTimeout);
  if (KERN_SUCCESS != err) {
    LOG_ERROR("Error sending get ports message %s (%x)\n",  mach_error_string(err), err);
    return false;
  }
  return true;
}

// Receive two communication ports from another process
bool
RecvPortsMessage(ReceivePort* receiver, mach_port_t* ports_in_sender, mach_port_t* ports_out_sender)
{
  MachReceiveMessage rcvPortsMsg;
  kern_return_t err = receiver->WaitForMessage(&rcvPortsMsg, kTimeout);
  if (KERN_SUCCESS != err) {
    LOG_ERROR("Error receiving get ports message %s (%x)\n",  mach_error_string(err), err);
  }
  if (rcvPortsMsg.GetTranslatedPort(0) == MACH_PORT_NULL) {
    LOG_ERROR("GetTranslatedPort(0) failed");
    return false;
  }
  *ports_in_sender = rcvPortsMsg.GetTranslatedPort(0);

  if (rcvPortsMsg.GetTranslatedPort(1) == MACH_PORT_NULL) {
    LOG_ERROR("GetTranslatedPort(1) failed");
    return false;
  }
  *ports_out_sender = rcvPortsMsg.GetTranslatedPort(1);
  return true;
}

// Send two communication ports to another process and receive two back
bool
RequestPorts(const MemoryPorts& request_ports,
             mach_port_t  ports_in_receiver,
             mach_port_t* ports_in_sender,
             mach_port_t* ports_out_sender,
             mach_port_t ports_out_receiver,
             PIDPair pid_pair)
{
  if (!SendPortsMessage(request_ports.mSender, ports_in_receiver, ports_out_receiver, pid_pair)) {
    return false;
  }
  return RecvPortsMessage(request_ports.mReceiver, ports_in_sender, ports_out_sender);
}

MemoryPorts*
GetMemoryPortsForPid(pid_t pid)
{
  gMutex.AssertCurrentThreadOwns();

  if (gMemoryCommPorts.find(pid) == gMemoryCommPorts.end()) {
    // We don't have the ports open to communicate with that pid, so we're going to
    // ask our parent process over IPC to set them up for us.
    if (gParentPid == 0) {
      // If we're the top level parent process, we have no parent to ask.
      LOG_ERROR("request for ports for pid %d, but we're the chrome process\n", pid);
      return nullptr;
    }
    const MemoryPorts& parent = gMemoryCommPorts[gParentPid];

    // Create two receiving ports in this process to send to the parent. One will be used for
    // for listening for incoming memory to be shared, the other for getting the Handle of
    // memory we share to the other process.
    auto* ports_in_receiver = new ReceivePort();
    auto* ports_out_receiver = new ReceivePort();
    mach_port_t raw_ports_in_sender, raw_ports_out_sender;
    if (!RequestPorts(parent,
                      ports_in_receiver->GetPort(),
                      &raw_ports_in_sender,
                      &raw_ports_out_sender,
                      ports_out_receiver->GetPort(),
                      PIDPair(getpid(), pid))) {
      LOG_ERROR("failed to request ports\n");
      return nullptr;
    }
    // Our parent process sent us two ports, one is for sending new memory to, the other
    // is for replying with the Handle when we receive new memory.
    auto* ports_in_sender = new MachPortSender(raw_ports_in_sender);
    auto* ports_out_sender = new MachPortSender(raw_ports_out_sender);
    SetupMachMemory(pid,
                    ports_in_receiver,
                    ports_in_sender,
                    ports_out_sender,
                    ports_out_receiver,
                    false);
    MOZ_ASSERT(gMemoryCommPorts.find(pid) != gMemoryCommPorts.end());
  }
  return &gMemoryCommPorts.at(pid);
}

// We just received a port representing a region of shared memory, reply to
// the process that set it with the mach_port_t that represents it in this process.
// That will be the Handle to be shared over normal IPC
void
HandleSharePortsMessage(MachReceiveMessage* rmsg, MemoryPorts* ports)
{
  mach_port_t port = rmsg->GetTranslatedPort(0);
  uint64_t* serial = reinterpret_cast<uint64_t*>(rmsg->GetData());
  MachSendMessage msg(kReturnIdMsg);
  // Construct the reply message, echoing the serial, and adding the port
  SharePortsReply replydata;
  replydata.port = port;
  replydata.serial = *serial;
  msg.SetData(&replydata, sizeof(SharePortsReply));
  kern_return_t err = ports->mSender->SendMessage(msg, kTimeout);
  if (KERN_SUCCESS != err) {
    LOG_ERROR("SendMessage failed 0x%x %s\n", err, mach_error_string(err));
  }
}

// We were asked by another process to get communications ports to some process. Return
// those ports via an IPC message.
bool
SendReturnPortsMsg(MachPortSender* sender,
                   mach_port_t raw_ports_in_sender,
                   mach_port_t raw_ports_out_sender)
{
  MachSendMessage getPortsMsg(kReturnPortsMsg);
  if (!getPortsMsg.AddDescriptor(MachMsgPortDescriptor(raw_ports_in_sender))) {
    LOG_ERROR("Adding descriptor to message failed");
    return false;
  }

  if (!getPortsMsg.AddDescriptor(MachMsgPortDescriptor(raw_ports_out_sender))) {
    LOG_ERROR("Adding descriptor to message failed");
    return false;
  }
  kern_return_t err = sender->SendMessage(getPortsMsg, kTimeout);
  if (KERN_SUCCESS != err) {
    LOG_ERROR("Error sending get ports message %s (%x)\n",  mach_error_string(err), err);
    return false;
  }
  return true;
}

// We were asked for communcations ports to a process that isn't us. Assuming that process
// is one of our children, forward that request on.
void
ForwardGetPortsMessage(MachReceiveMessage* rmsg, MemoryPorts* ports, PIDPair* pid_pair)
{
  if (rmsg->GetTranslatedPort(0) == MACH_PORT_NULL) {
    LOG_ERROR("GetTranslatedPort(0) failed");
    return;
  }
  if (rmsg->GetTranslatedPort(1) == MACH_PORT_NULL) {
    LOG_ERROR("GetTranslatedPort(1) failed");
    return;
  }
  mach_port_t raw_ports_in_sender, raw_ports_out_sender;
  MemoryPorts* requestedPorts = GetMemoryPortsForPid(pid_pair->mRequested);
  if (!requestedPorts) {
    LOG_ERROR("failed to find port for process\n");
    return;
  }
  if (!RequestPorts(*requestedPorts, rmsg->GetTranslatedPort(0), &raw_ports_in_sender,
                    &raw_ports_out_sender, rmsg->GetTranslatedPort(1), *pid_pair)) {
    LOG_ERROR("failed to request ports\n");
    return;
  }
  SendReturnPortsMsg(ports->mSender, raw_ports_in_sender, raw_ports_out_sender);
}

// We receieved a message asking us to get communications ports for another process
void
HandleGetPortsMessage(MachReceiveMessage* rmsg, MemoryPorts* ports)
{
  PIDPair* pid_pair;
  if (rmsg->GetDataLength() != sizeof(PIDPair)) {
    LOG_ERROR("Improperly formatted message\n");
    return;
  }
  pid_pair = reinterpret_cast<PIDPair*>(rmsg->GetData());
  if (pid_pair->mRequested != getpid()) {
    // This request is for ports to a process that isn't us, forward it to that process
    ForwardGetPortsMessage(rmsg, ports, pid_pair);
  } else {
    if (rmsg->GetTranslatedPort(0) == MACH_PORT_NULL) {
      LOG_ERROR("GetTranslatedPort(0) failed");
      return;
    }

    if (rmsg->GetTranslatedPort(1) == MACH_PORT_NULL) {
      LOG_ERROR("GetTranslatedPort(1) failed");
      return;
    }

    auto* ports_in_sender = new MachPortSender(rmsg->GetTranslatedPort(0));
    auto* ports_out_sender = new MachPortSender(rmsg->GetTranslatedPort(1));

    auto* ports_in_receiver = new ReceivePort();
    auto* ports_out_receiver = new ReceivePort();
    if (SendReturnPortsMsg(ports->mSender, ports_in_receiver->GetPort(), ports_out_receiver->GetPort())) {
      SetupMachMemory(pid_pair->mRequester,
                      ports_out_receiver,
                      ports_out_sender,
                      ports_in_sender,
                      ports_in_receiver,
                      false);
    }
  }
}

static void *
PortServerThread(void *argument)
{
  MemoryPorts* ports = static_cast<MemoryPorts*>(argument);
  MachReceiveMessage child_message;
  while (true) {
    MachReceiveMessage rmsg;
    kern_return_t err = ports->mReceiver->WaitForMessage(&rmsg, MACH_MSG_TIMEOUT_NONE);
    if (err != KERN_SUCCESS) {
      LOG_ERROR("Wait for message failed 0x%x %s\n", err, mach_error_string(err));
      continue;
    }
    if (rmsg.GetMessageID() == kShutdownMsg) {
      delete ports->mSender;
      delete ports->mReceiver;
      delete ports;
      return nullptr;
    }
    StaticMutexAutoLock smal(gMutex);
    switch (rmsg.GetMessageID()) {
    case kSharePortsMsg:
      HandleSharePortsMessage(&rmsg, ports);
      break;
    case kGetPortsMsg:
      HandleGetPortsMessage(&rmsg, ports);
      break;
    case kCleanupMsg:
     if (gParentPid == 0) {
       LOG_ERROR("Cleanup message not valid for parent process");
       continue;
     }

      pid_t* pid;
      if (rmsg.GetDataLength() != sizeof(pid_t)) {
        LOG_ERROR("Improperly formatted message\n");
        continue;
      }
      pid = reinterpret_cast<pid_t*>(rmsg.GetData());
      SharedMemoryBasic::CleanupForPid(*pid);
      break;
    default:
      LOG_ERROR("Unknown message\n");
    }
  }
}

void
SharedMemoryBasic::SetupMachMemory(pid_t pid,
                                   ReceivePort* listen_port,
                                   MachPortSender* listen_port_ack,
                                   MachPortSender* send_port,
                                   ReceivePort* send_port_ack,
                                   bool pidIsParent)
{
  StaticMutexAutoLock smal(gMutex);
  mozilla::ipc::SetupMachMemory(pid, listen_port, listen_port_ack, send_port, send_port_ack, pidIsParent);
}

void
SharedMemoryBasic::Shutdown()
{
  StaticMutexAutoLock smal(gMutex);

  for (auto& thread : gThreads) {
    MachSendMessage shutdownMsg(kShutdownMsg);
    thread.second.mPorts->mReceiver->SendMessageToSelf(shutdownMsg, kTimeout);
  }
  gThreads.clear();

  for (auto& memoryCommPort : gMemoryCommPorts) {
    delete memoryCommPort.second.mSender;
    delete memoryCommPort.second.mReceiver;
  }
  gMemoryCommPorts.clear();
}

void
SharedMemoryBasic::CleanupForPid(pid_t pid)
{
  if (gThreads.find(pid) == gThreads.end()) {
    return;
  }
  const ListeningThread& listeningThread = gThreads[pid];
  MachSendMessage shutdownMsg(kShutdownMsg);
  kern_return_t ret = listeningThread.mPorts->mReceiver->SendMessageToSelf(shutdownMsg, kTimeout);
  if (ret != KERN_SUCCESS) {
    LOG_ERROR("sending shutdown msg failed %s %x\n", mach_error_string(ret), ret);
  }
  gThreads.erase(pid);

  if (gParentPid == 0) {
    // We're the parent. Broadcast the cleanup message to everyone else.
    for (auto& memoryCommPort : gMemoryCommPorts) {
      MachSendMessage msg(kCleanupMsg);
      msg.SetData(&pid, sizeof(pid));
      // We don't really care if this fails, we could be trying to send to an already shut down proc
      memoryCommPort.second.mSender->SendMessage(msg, kTimeout);
    }
  }

  MemoryPorts& ports = gMemoryCommPorts[pid];
  delete ports.mSender;
  delete ports.mReceiver;
  gMemoryCommPorts.erase(pid);
}

SharedMemoryBasic::SharedMemoryBasic()
  : mPort(MACH_PORT_NULL)
  , mMemory(nullptr)
  , mOpenRights(RightsReadWrite)
{
}

SharedMemoryBasic::~SharedMemoryBasic()
{
  Unmap();
  CloseHandle();
}

bool
SharedMemoryBasic::SetHandle(const Handle& aHandle, OpenRights aRights)
{
  MOZ_ASSERT(mPort == MACH_PORT_NULL, "already initialized");

  mPort = aHandle;
  mOpenRights = aRights;
  return true;
}

static inline void*
toPointer(mach_vm_address_t address)
{
  return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

static inline mach_vm_address_t
toVMAddress(void* pointer)
{
  return static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(pointer));
}

bool
SharedMemoryBasic::Create(size_t size)
{
  MOZ_ASSERT(mPort == MACH_PORT_NULL, "already initialized");

  mach_vm_address_t address;

  kern_return_t kr = mach_vm_allocate(mach_task_self(), &address, round_page(size), VM_FLAGS_ANYWHERE);
  if (kr != KERN_SUCCESS) {
    LOG_ERROR("Failed to allocate mach_vm_allocate shared memory (%zu bytes). %s (%x)\n",
              size, mach_error_string(kr), kr);
    return false;
  }

  memory_object_size_t memoryObjectSize = round_page(size);

  kr = mach_make_memory_entry_64(mach_task_self(),
                                 &memoryObjectSize,
                                 address,
                                 VM_PROT_DEFAULT,
                                 &mPort,
                                 MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    LOG_ERROR("Failed to make memory entry (%zu bytes). %s (%x)\n",
              size, mach_error_string(kr), kr);
    return false;
  }

  mMemory = toPointer(address);
  Mapped(size);
  return true;
}

bool
SharedMemoryBasic::Map(size_t size)
{
  if (mMemory) {
    return true;
  }

  if (MACH_PORT_NULL == mPort) {
    return false;
  }

  kern_return_t kr;
  mach_vm_address_t address = 0;

  vm_prot_t vmProtection = VM_PROT_READ;
  if (mOpenRights == RightsReadWrite) {
    vmProtection |= VM_PROT_WRITE;
  }

  kr = mach_vm_map(mach_task_self(), &address, round_page(size), 0, VM_FLAGS_ANYWHERE,
                   mPort, 0, false, vmProtection, vmProtection, VM_INHERIT_NONE);
  if (kr != KERN_SUCCESS) {
    LOG_ERROR("Failed to map shared memory (%zu bytes) into %x, port %x. %s (%x)\n",
              size, mach_task_self(), mPort, mach_error_string(kr), kr);
    return false;
  }

  mMemory = toPointer(address);
  Mapped(size);
  return true;
}

bool
SharedMemoryBasic::ShareToProcess(base::ProcessId pid,
                                  Handle* aNewHandle)
{
  if (pid == getpid()) {
    *aNewHandle = mPort;
    return mach_port_mod_refs(mach_task_self(), *aNewHandle, MACH_PORT_RIGHT_SEND, 1) == KERN_SUCCESS;
  }
  StaticMutexAutoLock smal(gMutex);

   // Serially number the messages, to check whether
  // the reply we get was meant for us.
  static uint64_t serial = 0;
  uint64_t my_serial = serial;
  serial++;

  MemoryPorts* ports = GetMemoryPortsForPid(pid);
  if (!ports) {
    LOG_ERROR("Unable to get ports for process.\n");
    return false;
  }
  MachSendMessage smsg(kSharePortsMsg);
  smsg.AddDescriptor(MachMsgPortDescriptor(mPort, MACH_MSG_TYPE_COPY_SEND));
  smsg.SetData(&my_serial, sizeof(uint64_t));
  kern_return_t err = ports->mSender->SendMessage(smsg, kTimeout);
  if (err != KERN_SUCCESS) {
    LOG_ERROR("sending port failed %s %x\n", mach_error_string(err), err);
    return false;
  }
  MachReceiveMessage msg;
  err = ports->mReceiver->WaitForMessage(&msg, kTimeout);
  if (err != KERN_SUCCESS) {
    LOG_ERROR("short timeout didn't get an id %s %x\n", mach_error_string(err), err);
    err = ports->mReceiver->WaitForMessage(&msg, kLongTimeout);

    if (err != KERN_SUCCESS) {
      LOG_ERROR("long timeout didn't get an id %s %x\n", mach_error_string(err), err);
      return false;
    }
  }
  if (msg.GetDataLength() != sizeof(SharePortsReply)) {
    LOG_ERROR("Improperly formatted reply\n");
    return false;
  }
  SharePortsReply* msg_data = reinterpret_cast<SharePortsReply*>(msg.GetData());
  mach_port_t id = msg_data->port;
  uint64_t serial_check = msg_data->serial;
  if (serial_check != my_serial) {
    LOG_ERROR("Serials do not match up: %" PRIu64 " vs %" PRIu64 "", serial_check, my_serial);
    return false;
  }
  *aNewHandle = id;
  return true;
}

void
SharedMemoryBasic::Unmap()
{
  if (!mMemory) {
    return;
  }
  vm_address_t address = toVMAddress(mMemory);
  kern_return_t kr = vm_deallocate(mach_task_self(), address, round_page(mMappedSize));
  if (kr != KERN_SUCCESS) {
    LOG_ERROR("Failed to deallocate shared memory. %s (%x)\n",  mach_error_string(kr), kr);
    return;
  }
  mMemory = nullptr;
}

void
SharedMemoryBasic::CloseHandle()
{
  if (mPort != MACH_PORT_NULL) {
    mach_port_deallocate(mach_task_self(), mPort);
    mPort = MACH_PORT_NULL;
    mOpenRights = RightsReadWrite;
  }
}

bool
SharedMemoryBasic::IsHandleValid(const Handle& aHandle) const
{
  return aHandle > 0;
}

} // namespace ipc
} // namespace mozilla
