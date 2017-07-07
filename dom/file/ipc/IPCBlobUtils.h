/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IPCBlobUtils_h
#define mozilla_dom_IPCBlobUtils_h

#include "mozilla/dom/File.h"

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
 * - a temporary OS file: TemporaryBlobImpl
 * - a generic nsIInputStream: StreamBlobImpl
 * - an empty blob: EmptyBlobImpl
 * - more blobs combined together: MultipartBlobImpl
 * Each one of these implementations has a custom ::GetInternalStream method.
 * So, basically, each one has a different kind of nsIInputStream (nsFileStream,
 * nsIStringInputStream, SlicedInputStream, and so on).
 *
 * Another important point to keep in mind is that a Blob can be created on the
 * content process (for example: |new Blob([123])|) or it can be created on the
 * parent process and sent to content (a FilePicker creates Blobs and it runs on
 * the parent process).
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
 * PChildToParentStream. See more information in IPCStreamUtils.h.
 *
 * In order to populate IPCStream correctly, we use AutoIPCStream as documented
 * in IPCStreamUtils.h. Note that we use the 'delayed start' feature because,
 * often, the stream doesn't need to be read on the parent side.
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
 * nsIInputStream parent to child: PIPCBlobInputStream. This protocol keeps the
 * original nsIInputStream alive on the parent side, and gives its size and a
 * UUID to the child side. The child side creates a IPCBlobInputStream and that
 * is incapsulated into a StreamBlobImpl.
 *
 * The UUID is useful when the content process sends the same nsIInputStream
 * back to the parent process because, the only information it has to share is
 * the UUID. Each nsIInputStream sent via PIPCBlobInputStream, is registered
 * into the IPCBlobInputStreamStorage.
 *
 * On the content process side, IPCBlobInputStream is a special inputStream:
 * the only reliable methods are:
 * - nsIInputStream.available() - the size is shared by PIPCBlobInputStream
 *   actor.
 * - nsIIPCSerializableInputStream.serialize() - we can give back this stream to
 *   the parent because we know its UUID.
 * - nsICloneableInputStream.cloneable() and nsICloneableInputStream.clone() -
 *   this stream can be cloned. We just need to have a reference of the
 *   PIPCBlobInputStream actor and its UUID.
 * - nsIAsyncInputStream.asyncWait() - see next section.
 *
 * Any other method (read, readSegment and so on) will fail if asyncWait() is
 * not previously called (see the next section). Basically, this inputStream
 * cannot be used synchronously for any 'real' reading operation.
 *
 * When the parent receives the serialization of a IPCBlobInputStream, it is
 * able to retrieve the correct nsIInputStream using the UUID and
 * IPCBlobInputStreamStorage.
 *
 * Parent to Child Streams, FileReader and BlobURL
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The FileReader and BlobURL scenarios are described here.
 *
 * When content process needs to read data from a Blob sent from the parent
 * process, it must do it asynchronously using IPCBlobInputStream as a
 * nsIAsyncInputStream stream. This happens calling
 * IPCBlobInputStream.asyncWait(). At that point, the child actor will send a
 * StreamNeeded() IPC message to the parent side. When this is received, the
 * parent retrieves the 'real' stream from IPCBlobInputStreamStorage using the
 * UUID, it will serialize the 'real' stream, and it will send it to the child
 * side.
 *
 * When the 'real' stream is received (RecvStreamReady()), the asyncWait
 * callback will be executed and, from that moment, any IPCBlobInputStream
 * method will be forwarded to the 'real' stream ones. This means that the
 * reading will be available.
 *
 * DOM-File Thread
 * ~~~~~~~~~~~~~~~
 *
 * IPCBlobInputStreamChild actor can be created in any thread (sort of) and
 * their top-level IPDL protocol is PBackground. These actors are wrapped by 1
 * or more IPCBlobInputStream objects in order to expose nsIInputStream
 * interface and be thread-safe.
 *
 * But IPDL actors are not thread-safe and any SendFoo() method must be executed
 * on the owning thread. This means that this thread must be kept alive for the
 * life-time of the IPCBlobInputStream.
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
 * In order to avoid this, IPCBlobInputStreamChild are 'migrated' to a DOM-File
 * thread. This is done in this way:
 *
 * 1. If IPCBlobInputStreamChild actor is not already owned by DOM-File thread,
 *    it calls Send__delete__ in order to inform the parent side that we don't
 *    need this IPC channel on the current thread.
 * 3. IPCBlobInputStreamParent::Recv__delete__ is called on the parent side and
 *    the parent actor is deleted. Doing this we don't remove the UUID from
 *    IPCBlobInputStreamStorage.
 * 4. When IPCBlobInputStreamChild::ActorDestroy() is called, we are sure that
 *    the IPC channel is completely released. IPCBlobInputStreamThread is be
 *    used to assign IPCBlobInputStreamChild actor to the DOM-File thread.
 *    IPCBlobInputStreamThread::GetOrCreate() creates the DOM-File thread if it
 *    doesn't exist yet and it initializes PBackground on it if needed.
 * 5. IPCBlobInputStreamChild is reused on the DOM-File thread for the creation
 *    of a new IPCBlobInputStreamParent actor on the parent side. Doing this,
 *    IPCBlobInputStreamChild will now be owned by the DOM-File thread.
 * 6. When the new IPCBlobInputStreamParent actor is created, it will receive
 *    the same UUID of the previous parent actor. The nsIInputStream will be
 *    retrieved from IPCBlobInputStreamStorage.
 * 7. In order to avoid leaks, IPCBlobInputStreamStorage will monitor child
 *    processes and in case one of them dies, it will release the
 *    nsIInputStream objects belonging to that process.
 *
 * If any API wants to retrieve a 'real inputStream when the migration is in
 * progress, that operation is stored in a pending queue and processed at the
 * end of the migration.
 */

namespace mozilla {

namespace ipc {
class PBackgroundChild;
class PBackgroundParent;
}

namespace dom {

class IPCBlob;
class nsIContentChild;
class nsIContentParent;

namespace IPCBlobUtils {

already_AddRefed<BlobImpl>
Deserialize(const IPCBlob& aIPCBlob);

// These 4 methods serialize aBlobImpl into aIPCBlob using the right manager.

nsresult
Serialize(BlobImpl* aBlobImpl, nsIContentChild* aManager, IPCBlob& aIPCBlob);

nsresult
Serialize(BlobImpl* aBlobImpl, mozilla::ipc::PBackgroundChild* aManager,
          IPCBlob& aIPCBlob);

nsresult
Serialize(BlobImpl* aBlobImpl, nsIContentParent* aManager, IPCBlob& aIPCBlob);

nsresult
Serialize(BlobImpl* aBlobImpl, mozilla::ipc::PBackgroundParent* aManager,
          IPCBlob& aIPCBlob);

} // IPCBlobUtils
} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_IPCBlobUtils_h
