/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IPCBlobUtils_h
#define mozilla_dom_IPCBlobUtils_h

#include "mozilla/RefPtr.h"
#include "mozilla/dom/File.h"
#include "mozilla/ipc/IPDLParamTraits.h"

/*
 * Blobs and IPC
 * ~~~~~~~~~~~~~
 *
 * Simplifying, DOM Blob objects are chunks of data with a content type and a
 * size. DOM Files are Blobs with a name. They are are used in many APIs and
 * they can be cloned and sent cross threads and cross processes.
 *
 * If we see Blobs from a platform point of view, the main (and often, the only)
 * interesting part is how to retrieve data from it. This is done via
 * nsIInputStream and, except for a couple of important details, this stream is
 * used in the parent process.
 *
 * For this reason, when we consider the serialization of a blob via IPC
 * messages, the biggest effort is put in how to manage the nsInputStream
 * correctly. To serialize, we use the IPCBlob data struct: basically, the blob
 * properties (size, type, name if it's a file) and the nsIInputStream.
 *
 * Before talking about the nsIInputStream it's important to say that we have
 * different kinds of Blobs, based on the different kinds of sources. A non
 * exaustive list is:
 * - a memory buffer: MemoryBlobImpl
 * - a string: StringBlobImpl
 * - a real OS file: FileBlobImpl
 * - a generic nsIInputStream: StreamBlobImpl
 * - an empty blob: EmptyBlobImpl
 * - more blobs combined together: MultipartBlobImpl
 * Each one of these implementations has a custom ::CreateInputStream method.
 * So, basically, each one has a different kind of nsIInputStream (nsFileStream,
 * nsIStringInputStream, SlicedInputStream, and so on).
 *
 * Another important point to keep in mind is that a Blob can be created on the
 * content process (for example: |new Blob([123])|) or it can be created on the
 * parent process and sent to content (a FilePicker creates Blobs and it runs on
 * the parent process).
 *
 * DocumentLoadListener uses blobs to serialize the POST data back to the
 * content process (for insertion into session history). This lets it correctly
 * handle OS files by reference, and avoid copying the underlying buffer data
 * unless it is read. This can hopefully be removed once SessionHistory is
 * handled in the parent process.
 *
 * Child to Parent Blob Serialization
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * When a document creates a blob, this can be sent, for different reasons to
 * the parent process. For instance it can be sent as part of a FormData, or it
 * can be converted to a BlobURL and broadcasted to any other existing
 * processes.
 *
 * When this happens, we use the IPCStream data struct for the serialization
 * of the nsIInputStream. This means that, if the stream is fully serializable
 * and its size is lower than 1Mb, we are able to recreate the stream completely
 * on the parent side. This happens, basically with any kind of child-to-parent
 * stream except for huge memory streams. In this case we end up using
 * DataPipe. See more information in IPCStreamUtils.h.
 *
 * In order to populate IPCStream correctly, we use SerializeIPCStream as
 * documented in IPCStreamUtils.h.
 *
 * Parent to Child Blob Serialization
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This scenario is common when we talk about Blobs pointing to real files:
 * HTMLInputElement (type=file), or Entries API, DataTransfer and so on. But we
 * also have this scenario when a content process creates a Blob and it
 * broadcasts it because of a BlobURL or because BroadcastChannel API is used.
 *
 * The approach here is this: normally, the content process doesn't really read
 * data from the blob nsIInputStream. The content process needs to have the
 * nsIInputStream and be able to send it back to the parent process when the
 * "real" work needs to be done. This is true except for 2 usecases: FileReader
 * API and BlobURL usage. So, if we ignore these 2, normally, the parent sends a
 * blob nsIInputStream to a content process, and then, it will receive it back
 * in order to do some networking, or whatever.
 *
 * For this reason, IPCBlobUtils uses a particular protocol for serializing
 * nsIInputStream parent to child: PRemoteLazyInputStream. This protocol keeps
 * the original nsIInputStream alive on the parent side, and gives its size and
 * a UUID to the child side. The child side creates a RemoteLazyInputStream and
 * that is incapsulated into a StreamBlobImpl.
 *
 * The UUID is useful when the content process sends the same nsIInputStream
 * back to the parent process because, the only information it has to share is
 * the UUID. Each nsIInputStream sent via PRemoteLazyInputStream, is registered
 * into the RemoteLazyInputStreamStorage.
 *
 * On the content process side, RemoteLazyInputStream is a special inputStream:
 * the only reliable methods are:
 * - nsIInputStream.available() - the size is shared by PRemoteLazyInputStream
 *   actor.
 * - nsIIPCSerializableInputStream.serialize() - we can give back this stream to
 *   the parent because we know its UUID.
 * - nsICloneableInputStream.cloneable() and nsICloneableInputStream.clone() -
 *   this stream can be cloned. We just need to have a reference of the
 *   PRemoteLazyInputStream actor and its UUID.
 * - nsIAsyncInputStream.asyncWait() - see next section.
 *
 * Any other method (read, readSegment and so on) will fail if asyncWait() is
 * not previously called (see the next section). Basically, this inputStream
 * cannot be used synchronously for any 'real' reading operation.
 *
 * When the parent receives the serialization of a RemoteLazyInputStream, it is
 * able to retrieve the correct nsIInputStream using the UUID and
 * RemoteLazyInputStreamStorage.
 *
 * Parent to Child Streams, FileReader and BlobURL
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The FileReader and BlobURL scenarios are described here.
 *
 * When content process needs to read data from a Blob sent from the parent
 * process, it must do it asynchronously using RemoteLazyInputStream as a
 * nsIAsyncInputStream stream. This happens calling
 * RemoteLazyInputStream.asyncWait(). At that point, the child actor will send a
 * StreamNeeded() IPC message to the parent side. When this is received, the
 * parent retrieves the 'real' stream from RemoteLazyInputStreamStorage using
 * the UUID, it will serialize the 'real' stream, and it will send it to the
 * child side.
 *
 * When the 'real' stream is received (RecvStreamReady()), the asyncWait
 * callback will be executed and, from that moment, any RemoteLazyInputStream
 * method will be forwarded to the 'real' stream ones. This means that the
 * reading will be available.
 *
 * RemoteLazyInputStream Thread
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * RemoteLazyInputStreamChild actor can be created in any thread (sort of) and
 * their top-level IPDL protocol is PBackground. These actors are wrapped by 1
 * or more RemoteLazyInputStream objects in order to expose nsIInputStream
 * interface and be thread-safe.
 *
 * But IPDL actors are not thread-safe and any SendFoo() method must be executed
 * on the owning thread. This means that this thread must be kept alive for the
 * life-time of the RemoteLazyInputStream.
 *
 * In doing this, there are 2 main issues:
 * a. if a remote Blob is created on a worker (because of a
 *    BroadcastChannel/MessagePort for instance) and it sent to the main-thread
 *    via PostMessage(), we have to keep that worker alive.
 * b. if the remote Blob is created on the main-thread, any SendFoo() has to be
 *    executed on the main-thread. This is true also when the inputStream is
 *    used on another thread (note that nsIInputStream could do I/O and usually
 *    they are used on special I/O threads).
 *
 * In order to avoid this, RemoteLazyInputStreamChild are 'migrated' to a
 * DOM-File thread. This is done in this way:
 *
 * 1. If RemoteLazyInputStreamChild actor is not already owned by DOM-File
 *    thread, it calls Send__delete__ in order to inform the parent side that we
 *    don't need this IPC channel on the current thread.
 * 2. A new RemoteLazyInputStreamChild is created. RemoteLazyInputStreamThread
 *    is used to assign this actor to the DOM-File thread.
 *    RemoteLazyInputStreamThread::GetOrCreate() creates the DOM-File thread if
 *    it doesn't exist yet. Pending operations and RemoteLazyInputStreams are
 *    moved onto the new actor.
 * 3. RemoteLazyInputStreamParent::Recv__delete__ is called on the parent side
 *    and the parent actor is deleted. Doing this we don't remove the UUID from
 *    RemoteLazyInputStreamStorage.
 * 4. The RemoteLazyInputStream constructor is sent with the new
 *    RemoteLazyInputStreamChild actor, with the DOM-File thread's PBackground
 *    as its manager.
 * 5. When the new RemoteLazyInputStreamParent actor is created, it will receive
 *    the same UUID of the previous parent actor. The nsIInputStream will be
 *    retrieved from RemoteLazyInputStreamStorage.
 * 6. In order to avoid leaks, RemoteLazyInputStreamStorage will monitor child
 *    processes and in case one of them dies, it will release the
 *    nsIInputStream objects belonging to that process.
 *
 * If any API wants to retrieve a 'real inputStream when the migration is in
 * progress, that operation is stored in a pending queue and processed at the
 * end of the migration.
 *
 * IPCBlob and nsIAsyncInputStream
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * RemoteLazyInputStream is always async. If the remote inputStream is not
 * async, RemoteLazyInputStream will create a pipe stream around it in order to
 * be consistently async.
 *
 * Slicing IPCBlob
 * ~~~~~~~~~~~~~~~
 *
 * Normally, slicing a blob consists of the creation of a new Blob, with a
 * SlicedInputStream() wrapping a clone of the original inputStream. But this
 * approach is extremely inefficient with IPCBlob, because it could be that we
 * wrap the pipe stream and not the remote inputStream (See the previous section
 * of this documentation). If we end up doing so, also if the remote
 * inputStream is seekable, the pipe will not be, and in order to reach the
 * starting point, SlicedInputStream will do consecutive read()s.
 *
 * This problem is fixed implmenting nsICloneableWithRange in
 * RemoteLazyInputStream and using cloneWithRange() when a StreamBlobImpl is
 * sliced. When the remote stream is received, it will be sliced directly.
 *
 * If we want to represent the hierarchy of the InputStream classes, instead
 * of having: |SlicedInputStream(RemoteLazyInputStream(Async
 * Pipe(RemoteStream)))|, we have: |RemoteLazyInputStream(Async
 * Pipe(SlicedInputStream(RemoteStream)))|.
 *
 * When RemoteLazyInputStream is serialized and sent to the parent process,
 * start and range are sent too and SlicedInputStream is used in the parent side
 * as well.
 *
 * Socket Process
 * ~~~~~~~~~~~~~~
 *
 * The socket process is a separate process used to do networking operations.
 * When a website sends a blob as the body of a POST/PUT request, we need to
 * send the corresponding RemoteLazyInputStream to the socket process.
 *
 * This is the only serialization of RemoteLazyInputStream from parent to child
 * process and it works _only_ for the socket process. Do not expose this
 * serialization to PContent or PBackground or any other top-level IPDL protocol
 * without a DOM File peer review!
 *
 * The main difference between Socket Process is that DOM-File thread is not
 * used. Here is a list of reasons:
 * - DOM-File moves the ownership of the RemoteLazyInputStream actors to
 *   PBackground, but in the Socket Process we don't have PBackground (yet?)
 * - Socket Process is a stable process with a simple life-time configuration:
 *   we can keep the actors on the main-thread because no Workers are involved.
 */

namespace mozilla::dom {

class IPCBlob;

namespace IPCBlobUtils {

already_AddRefed<BlobImpl> Deserialize(const IPCBlob& aIPCBlob);

nsresult Serialize(BlobImpl* aBlobImpl, IPCBlob& aIPCBlob);

}  // namespace IPCBlobUtils
}  // namespace mozilla::dom

namespace IPC {

// ParamTraits implementation for BlobImpl. N.B: If the original BlobImpl cannot
// be successfully serialized, a warning will be produced and a nullptr will be
// sent over the wire. When Read()-ing a BlobImpl,
// __always make sure to handle null!__
template <>
struct ParamTraits<mozilla::dom::BlobImpl*> {
  static void Write(IPC::MessageWriter* aWriter,
                    mozilla::dom::BlobImpl* aParam);
  static bool Read(IPC::MessageReader* aReader,
                   RefPtr<mozilla::dom::BlobImpl>* aResult);
};

}  // namespace IPC

#endif  // mozilla_dom_IPCBlobUtils_h
