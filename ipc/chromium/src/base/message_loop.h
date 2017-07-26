/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_H_
#define BASE_MESSAGE_LOOP_H_

#include <deque>
#include <queue>
#include <string>
#include <vector>
#include <map>

#include "base/lock.h"
#include "base/message_pump.h"
#include "base/observer_list.h"

#if defined(OS_WIN)
// We need this to declare base::MessagePumpWin::Dispatcher, which we should
// really just eliminate.
#include "base/message_pump_win.h"
#elif defined(OS_POSIX)
#include "base/message_pump_libevent.h"
#endif

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

class nsISerialEventTarget;
class nsIThread;

namespace mozilla {
namespace ipc {

class DoWorkRunnable;

} /* namespace ipc */
} /* namespace mozilla */

// A MessageLoop is used to process events for a particular thread.  There is
// at most one MessageLoop instance per thread.
//
// Events include at a minimum Task instances submitted to PostTask or those
// managed by TimerManager.  Depending on the type of message pump used by the
// MessageLoop other events such as UI messages may be processed.  On Windows
// APC calls (as time permits) and signals sent to a registered set of HANDLEs
// may also be processed.
//
// NOTE: Unless otherwise specified, a MessageLoop's methods may only be called
// on the thread where the MessageLoop's Run method executes.
//
// NOTE: MessageLoop has task reentrancy protection.  This means that if a
// task is being processed, a second task cannot start until the first task is
// finished.  Reentrancy can happen when processing a task, and an inner
// message pump is created.  That inner pump then processes native messages
// which could implicitly start an inner task.  Inner message pumps are created
// with dialogs (DialogBox), common dialogs (GetOpenFileName), OLE functions
// (DoDragDrop), printer functions (StartDoc) and *many* others.
//
// Sample workaround when inner task processing is needed:
//   bool old_state = MessageLoop::current()->NestableTasksAllowed();
//   MessageLoop::current()->SetNestableTasksAllowed(true);
//   HRESULT hr = DoDragDrop(...); // Implicitly runs a modal message loop here.
//   MessageLoop::current()->SetNestableTasksAllowed(old_state);
//   // Process hr  (the result returned by DoDragDrop().
//
// Please be SURE your task is reentrant (nestable) and all global variables
// are stable and accessible before calling SetNestableTasksAllowed(true).
//
class MessageLoop : public base::MessagePump::Delegate {

  friend class mozilla::ipc::DoWorkRunnable;

public:
  // A DestructionObserver is notified when the current MessageLoop is being
  // destroyed.  These obsevers are notified prior to MessageLoop::current()
  // being changed to return NULL.  This gives interested parties the chance to
  // do final cleanup that depends on the MessageLoop.
  //
  // NOTE: Any tasks posted to the MessageLoop during this notification will
  // not be run.  Instead, they will be deleted.
  //
  class DestructionObserver {
   public:
    virtual ~DestructionObserver() {}
    virtual void WillDestroyCurrentMessageLoop() = 0;
  };

  // Add a DestructionObserver, which will start receiving notifications
  // immediately.
  void AddDestructionObserver(DestructionObserver* destruction_observer);

  // Remove a DestructionObserver.  It is safe to call this method while a
  // DestructionObserver is receiving a notification callback.
  void RemoveDestructionObserver(DestructionObserver* destruction_observer);

  // The "PostTask" family of methods call the task's Run method asynchronously
  // from within a message loop at some point in the future.
  //
  // With the PostTask variant, tasks are invoked in FIFO order, inter-mixed
  // with normal UI or IO event processing.  With the PostDelayedTask variant,
  // tasks are called after at least approximately 'delay_ms' have elapsed.
  //
  // The NonNestable variants work similarly except that they promise never to
  // dispatch the task from a nested invocation of MessageLoop::Run.  Instead,
  // such tasks get deferred until the top-most MessageLoop::Run is executing.
  //
  // The MessageLoop takes ownership of the Task, and deletes it after it has
  // been Run().
  //
  // New tasks should not be posted after the invocation of a MessageLoop's
  // Run method. Otherwise, they may fail to actually run. Callers should check
  // if the MessageLoop is processing tasks if necessary by calling
  // IsAcceptingTasks().
  //
  // NOTE: These methods may be called on any thread.  The Task will be invoked
  // on the thread that executes MessageLoop::Run().

  bool IsAcceptingTasks() const { return !shutting_down_; }

  void PostTask(already_AddRefed<nsIRunnable> task);

  void PostDelayedTask(already_AddRefed<nsIRunnable> task, int delay_ms);

  // PostIdleTask is not thread safe and should be called on this thread
  void PostIdleTask(already_AddRefed<nsIRunnable> task);

  // Run the message loop.
  void Run();

  // Signals the Run method to return after it is done processing all pending
  // messages.  This method may only be called on the same thread that called
  // Run, and Run must still be on the call stack.
  //
  // Use QuitTask if you need to Quit another thread's MessageLoop, but note
  // that doing so is fairly dangerous if the target thread makes nested calls
  // to MessageLoop::Run.  The problem being that you won't know which nested
  // run loop you are quiting, so be careful!
  //
  void Quit();

  // Invokes Quit on the current MessageLoop when run.  Useful to schedule an
  // arbitrary MessageLoop to Quit.
  class QuitTask : public mozilla::Runnable {
   public:
    QuitTask() : mozilla::Runnable("QuitTask") {}
    NS_IMETHOD Run() override {
      MessageLoop::current()->Quit();
      return NS_OK;
    }
  };

  // Return an XPCOM-compatible event target for this thread.
  nsISerialEventTarget* SerialEventTarget();

  // A MessageLoop has a particular type, which indicates the set of
  // asynchronous events it may process in addition to tasks and timers.
  //
  // TYPE_DEFAULT
  //   This type of ML only supports tasks and timers.
  //
  // TYPE_UI
  //   This type of ML also supports native UI events (e.g., Windows messages).
  //   See also MessageLoopForUI.
  //
  // TYPE_IO
  //   This type of ML also supports asynchronous IO.  See also
  //   MessageLoopForIO.
  //
  // TYPE_MOZILLA_CHILD
  //   This type of ML is used in Mozilla child processes which initialize
  //   XPCOM and use the gecko event loop.
  //
  // TYPE_MOZILLA_PARENT
  //   This type of ML is used in Mozilla parent processes which initialize
  //   XPCOM and use the gecko event loop.
  //
  // TYPE_MOZILLA_NONMAINTHREAD
  //   This type of ML is used in Mozilla parent processes which initialize
  //   XPCOM and use the nsThread event loop.
  //
  // TYPE_MOZILLA_NONMAINUITHREAD
  //   This type of ML is used in Mozilla processes which initialize XPCOM
  //   and use TYPE_UI loop logic.
  //
  enum Type {
    TYPE_DEFAULT,
    TYPE_UI,
    TYPE_IO,
    TYPE_MOZILLA_CHILD,
    TYPE_MOZILLA_PARENT,
    TYPE_MOZILLA_NONMAINTHREAD,
    TYPE_MOZILLA_NONMAINUITHREAD,
    TYPE_MOZILLA_ANDROID_UI
  };

  // Normally, it is not necessary to instantiate a MessageLoop.  Instead, it
  // is typical to make use of the current thread's MessageLoop instance.
  explicit MessageLoop(Type type = TYPE_DEFAULT, nsIThread* aThread = nullptr);
  ~MessageLoop();

  // Returns the type passed to the constructor.
  Type type() const { return type_; }

  // Unique, non-repeating ID for this message loop.
  int32_t id() const { return id_; }

  // Optional call to connect the thread name with this loop.
  void set_thread_name(const std::string& aThreadName) {
    DCHECK(thread_name_.empty()) << "Should not rename this thread!";
    thread_name_ = aThreadName;
  }
  const std::string& thread_name() const { return thread_name_; }

  // Returns the MessageLoop object for the current thread, or null if none.
  static MessageLoop* current();

  static void set_current(MessageLoop* loop);

  // Enables or disables the recursive task processing. This happens in the case
  // of recursive message loops. Some unwanted message loop may occurs when
  // using common controls or printer functions. By default, recursive task
  // processing is disabled.
  //
  // The specific case where tasks get queued is:
  // - The thread is running a message loop.
  // - It receives a task #1 and execute it.
  // - The task #1 implicitly start a message loop, like a MessageBox in the
  //   unit test. This can also be StartDoc or GetSaveFileName.
  // - The thread receives a task #2 before or while in this second message
  //   loop.
  // - With NestableTasksAllowed set to true, the task #2 will run right away.
  //   Otherwise, it will get executed right after task #1 completes at "thread
  //   message loop level".
  void SetNestableTasksAllowed(bool allowed);
  void ScheduleWork();
  bool NestableTasksAllowed() const;

  // Enables or disables the restoration during an exception of the unhandled
  // exception filter that was active when Run() was called. This can happen
  // if some third party code call SetUnhandledExceptionFilter() and never
  // restores the previous filter.
  void set_exception_restoration(bool restore) {
    exception_restoration_ = restore;
  }

#if defined(OS_WIN)
  void set_os_modal_loop(bool os_modal_loop) {
    os_modal_loop_ = os_modal_loop;
  }

  bool & os_modal_loop() {
    return os_modal_loop_;
  }
#endif  // OS_WIN

  // Set the timeouts for background hang monitoring.
  // A value of 0 indicates there is no timeout.
  void set_hang_timeouts(uint32_t transient_timeout_ms,
                         uint32_t permanent_timeout_ms) {
    transient_hang_timeout_ = transient_timeout_ms;
    permanent_hang_timeout_ = permanent_timeout_ms;
  }
  uint32_t transient_hang_timeout() const {
    return transient_hang_timeout_;
  }
  uint32_t permanent_hang_timeout() const {
    return permanent_hang_timeout_;
  }

  //----------------------------------------------------------------------------
 protected:
  struct RunState {
    // Used to count how many Run() invocations are on the stack.
    int run_depth;

    // Used to record that Quit() was called, or that we should quit the pump
    // once it becomes idle.
    bool quit_received;

#if defined(OS_WIN)
    base::MessagePumpWin::Dispatcher* dispatcher;
#endif
  };

  class AutoRunState : RunState {
   public:
    explicit AutoRunState(MessageLoop* loop);
    ~AutoRunState();
   private:
    MessageLoop* loop_;
    RunState* previous_state_;
  };

  // This structure is copied around by value.
  struct PendingTask {
    nsCOMPtr<nsIRunnable> task;        // The task to run.
    base::TimeTicks delayed_run_time;  // The time when the task should be run.
    int sequence_num;                  // Secondary sort key for run time.
    bool nestable;                     // OK to dispatch from a nested loop.

    PendingTask(already_AddRefed<nsIRunnable> aTask, bool aNestable)
        : task(aTask), sequence_num(0), nestable(aNestable) {
    }

    PendingTask(PendingTask&& aOther)
        : task(aOther.task.forget()),
          delayed_run_time(aOther.delayed_run_time),
          sequence_num(aOther.sequence_num),
          nestable(aOther.nestable) {
    }

    // std::priority_queue<T>::top is dumb, so we have to have this.
    PendingTask(const PendingTask& aOther)
        : task(aOther.task),
          delayed_run_time(aOther.delayed_run_time),
          sequence_num(aOther.sequence_num),
          nestable(aOther.nestable) {
    }
    PendingTask& operator=(const PendingTask& aOther)
    {
      task = aOther.task;
      delayed_run_time = aOther.delayed_run_time;
      sequence_num = aOther.sequence_num;
      nestable = aOther.nestable;
      return *this;
    }

    // Used to support sorting.
    bool operator<(const PendingTask& other) const;
  };

  typedef std::queue<PendingTask> TaskQueue;
  typedef std::priority_queue<PendingTask> DelayedTaskQueue;

#if defined(OS_WIN)
  base::MessagePumpWin* pump_win() {
    return static_cast<base::MessagePumpWin*>(pump_.get());
  }
#elif defined(OS_POSIX)
  base::MessagePumpLibevent* pump_libevent() {
    return static_cast<base::MessagePumpLibevent*>(pump_.get());
  }
#endif

  // A function to encapsulate all the exception handling capability in the
  // stacks around the running of a main message loop.  It will run the message
  // loop in a SEH try block or not depending on the set_SEH_restoration()
  // flag.
  void RunHandler();

  // A surrounding stack frame around the running of the message loop that
  // supports all saving and restoring of state, as is needed for any/all (ugly)
  // recursive calls.
  void RunInternal();

  // Called to process any delayed non-nestable tasks.
  bool ProcessNextDelayedNonNestableTask();

  //----------------------------------------------------------------------------
  // Run a work_queue_ task or new_task, and delete it (if it was processed by
  // PostTask). If there are queued tasks, the oldest one is executed and
  // new_task is queued. new_task is optional and can be NULL. In this NULL
  // case, the method will run one pending task (if any exist). Returns true if
  // it executes a task.  Queued tasks accumulate only when there is a
  // non-nestable task currently processing, in which case the new_task is
  // appended to the list work_queue_.  Such re-entrancy generally happens when
  // an unrequested message pump (typical of a native dialog) is executing in
  // the context of a task.
  bool QueueOrRunTask(already_AddRefed<nsIRunnable> new_task);

  // Runs the specified task and deletes it.
  void RunTask(already_AddRefed<nsIRunnable> task);

  // Calls RunTask or queues the pending_task on the deferred task list if it
  // cannot be run right now.  Returns true if the task was run.
  bool DeferOrRunPendingTask(PendingTask&& pending_task);

  // Adds the pending task to delayed_work_queue_.
  void AddToDelayedWorkQueue(const PendingTask& pending_task);

  // Load tasks from the incoming_queue_ into work_queue_ if the latter is
  // empty.  The former requires a lock to access, while the latter is directly
  // accessible on this thread.
  void ReloadWorkQueue();

  // Delete tasks that haven't run yet without running them.  Used in the
  // destructor to make sure all the task's destructors get called.  Returns
  // true if some work was done.
  bool DeletePendingTasks();

  // Post a task to our incomming queue.
  void PostTask_Helper(already_AddRefed<nsIRunnable> task, int delay_ms);

  // base::MessagePump::Delegate methods:
  virtual bool DoWork() override;
  virtual bool DoDelayedWork(base::TimeTicks* next_delayed_work_time) override;
  virtual bool DoIdleWork() override;

  Type type_;
  int32_t id_;

  // A list of tasks that need to be processed by this instance.  Note that
  // this queue is only accessed (push/pop) by our current thread.
  TaskQueue work_queue_;

  // Contains delayed tasks, sorted by their 'delayed_run_time' property.
  DelayedTaskQueue delayed_work_queue_;

  // A queue of non-nestable tasks that we had to defer because when it came
  // time to execute them we were in a nested message loop.  They will execute
  // once we're out of nested message loops.
  TaskQueue deferred_non_nestable_work_queue_;

  RefPtr<base::MessagePump> pump_;

  base::ObserverList<DestructionObserver> destruction_observers_;

  // A recursion block that prevents accidentally running additonal tasks when
  // insider a (accidentally induced?) nested message pump.
  bool nestable_tasks_allowed_;

  bool exception_restoration_;

  std::string thread_name_;

  // A null terminated list which creates an incoming_queue of tasks that are
  // aquired under a mutex for processing on this instance's thread. These tasks
  // have not yet been sorted out into items for our work_queue_ vs items that
  // will be handled by the TimerManager.
  TaskQueue incoming_queue_;
  // Protect access to incoming_queue_.
  Lock incoming_queue_lock_;

  RunState* state_;
  int run_depth_base_;
  bool shutting_down_;

#if defined(OS_WIN)
  // Should be set to true before calling Windows APIs like TrackPopupMenu, etc
  // which enter a modal message loop.
  bool os_modal_loop_;
#endif

  // Timeout values for hang monitoring
  uint32_t transient_hang_timeout_;
  uint32_t permanent_hang_timeout_;

  // The next sequence number to use for delayed tasks.
  int next_sequence_num_;

  class EventTarget;
  RefPtr<EventTarget> mEventTarget;

  DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};

//-----------------------------------------------------------------------------
// MessageLoopForUI extends MessageLoop with methods that are particular to a
// MessageLoop instantiated with TYPE_UI.
//
// This class is typically used like so:
//   MessageLoopForUI::current()->...call some method...
//
class MessageLoopForUI : public MessageLoop {
 public:
  explicit MessageLoopForUI(Type aType=TYPE_UI) : MessageLoop(aType) {
  }

  // Returns the MessageLoopForUI of the current thread.
  static MessageLoopForUI* current() {
    MessageLoop* loop = MessageLoop::current();
    if (!loop)
      return NULL;
    Type type = loop->type();
    DCHECK(type == MessageLoop::TYPE_UI ||
           type == MessageLoop::TYPE_MOZILLA_PARENT ||
           type == MessageLoop::TYPE_MOZILLA_CHILD);
    return static_cast<MessageLoopForUI*>(loop);
  }

#if defined(OS_WIN)
  typedef base::MessagePumpWin::Dispatcher Dispatcher;
  typedef base::MessagePumpWin::Observer Observer;

  // Please see MessagePumpWin for definitions of these methods.
  void Run(Dispatcher* dispatcher);
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void WillProcessMessage(const MSG& message);
  void DidProcessMessage(const MSG& message);
  void PumpOutPendingPaintMessages();

 protected:
  // TODO(rvargas): Make this platform independent.
  base::MessagePumpForUI* pump_ui() {
    return static_cast<base::MessagePumpForUI*>(pump_.get());
  }
#endif  // defined(OS_WIN)
};

// Do not add any member variables to MessageLoopForUI!  This is important b/c
// MessageLoopForUI is often allocated via MessageLoop(TYPE_UI).  Any extra
// data that you need should be stored on the MessageLoop's pump_ instance.
COMPILE_ASSERT(sizeof(MessageLoop) == sizeof(MessageLoopForUI),
               MessageLoopForUI_should_not_have_extra_member_variables);

//-----------------------------------------------------------------------------
// MessageLoopForIO extends MessageLoop with methods that are particular to a
// MessageLoop instantiated with TYPE_IO.
//
// This class is typically used like so:
//   MessageLoopForIO::current()->...call some method...
//
class MessageLoopForIO : public MessageLoop {
 public:
  MessageLoopForIO() : MessageLoop(TYPE_IO) {
  }

  // Returns the MessageLoopForIO of the current thread.
  static MessageLoopForIO* current() {
    MessageLoop* loop = MessageLoop::current();
    DCHECK_EQ(MessageLoop::TYPE_IO, loop->type());
    return static_cast<MessageLoopForIO*>(loop);
  }

#if defined(OS_WIN)
  typedef base::MessagePumpForIO::IOHandler IOHandler;
  typedef base::MessagePumpForIO::IOContext IOContext;

  // Please see MessagePumpWin for definitions of these methods.
  void RegisterIOHandler(HANDLE file_handle, IOHandler* handler);
  bool WaitForIOCompletion(DWORD timeout, IOHandler* filter);

 protected:
  // TODO(rvargas): Make this platform independent.
  base::MessagePumpForIO* pump_io() {
    return static_cast<base::MessagePumpForIO*>(pump_.get());
  }

#elif defined(OS_POSIX)
  typedef base::MessagePumpLibevent::Watcher Watcher;
  typedef base::MessagePumpLibevent::FileDescriptorWatcher
      FileDescriptorWatcher;
  typedef base::LineWatcher LineWatcher;

  enum Mode {
    WATCH_READ = base::MessagePumpLibevent::WATCH_READ,
    WATCH_WRITE = base::MessagePumpLibevent::WATCH_WRITE,
    WATCH_READ_WRITE = base::MessagePumpLibevent::WATCH_READ_WRITE
  };

  // Please see MessagePumpLibevent for definition.
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           Mode mode,
                           FileDescriptorWatcher *controller,
                           Watcher *delegate);

  typedef base::MessagePumpLibevent::SignalEvent SignalEvent;
  typedef base::MessagePumpLibevent::SignalWatcher SignalWatcher;
  bool CatchSignal(int sig,
                   SignalEvent* sigevent,
                   SignalWatcher* delegate);

#endif  // defined(OS_POSIX)
};

// Do not add any member variables to MessageLoopForIO!  This is important b/c
// MessageLoopForIO is often allocated via MessageLoop(TYPE_IO).  Any extra
// data that you need should be stored on the MessageLoop's pump_ instance.
COMPILE_ASSERT(sizeof(MessageLoop) == sizeof(MessageLoopForIO),
               MessageLoopForIO_should_not_have_extra_member_variables);

#endif  // BASE_MESSAGE_LOOP_H_
