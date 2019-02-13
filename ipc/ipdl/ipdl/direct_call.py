# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Our long term goal is to burn this list down, so new entries should be added
# extremely sparingly and only with a very good reason! You must have an IPC
# peer's r+ to add something new!

# {(Protocol, side): (Class, HeaderFile)}
DIRECT_CALL_OVERRIDES = {
    ("PAPZ", "parent"): (
        "RemoteContentController", "mozilla/layers/RemoteContentController.h"
    ),

    ("PBackgroundMutableFile", "parent"): (
        "BackgroundMutableFileParentBase", "mozilla/dom/filehandle/ActorsParent.h"
    ),

    ("PBrowser", "parent"): ("TabParent", "mozilla/dom/TabParent.h"),

    ("PChromiumCDM", "parent"): ("ChromiumCDMParent", "ChromiumCDMParent.h"),

    ("PCompositorBridge", "parent"): (
        "CompositorBridgeParentBase", "mozilla/layers/CompositorBridgeParent.h"
    ),

    ("PContentPermissionRequest", "child"): (
        "RemotePermissionRequest", "nsContentPermissionHelper.h"
    ),

    ("PFileSystemRequest", "child"): (
        "FileSystemTaskChildBase", "mozilla/dom/FileSystemTaskBase.h"
    ),

    ("PGMP", "child"): ("GMPChild", "GMPChild.h"),
    ("PGMP", "parent"): ("GMPParent", "GMPParent.h"),
    ("PGMPContent", "child"): ("GMPContentChild", "GMPContentChild.h"),
    ("PGMPStorage", "child"): ("GMPStorageChild", "GMPStorageChild.h"),
    ("PGMPTimer", "child"): ("GMPTimerChild", "GMPTimerChild.h"),
    ("PGMPTimer", "parent"): ("GMPTimerParent", "GMPTimerParent.h"),
    ("PGMPVideoEncoder", "child"): ("GMPVideoEncoderChild", "GMPVideoEncoderChild.h"),
    ("PGMPVideoDecoder", "child"): ("GMPVideoDecoderChild", "GMPVideoDecoderChild.h"),

    ("PIPCBlobInputStream", "child"): (
        "mozilla::dom::IPCBlobInputStreamChild", "mozilla/dom/ipc/IPCBlobInputStreamChild.h"
    ),
    ("PIPCBlobInputStream", "parent"): (
        "mozilla::dom::IPCBlobInputStreamParent", "mozilla/dom/ipc/IPCBlobInputStreamParent.h"
    ),

    ("PLoginReputation", "parent"): ("LoginReputationParent", "mozilla/LoginReputationIPC.h"),

    ("PMedia", "child"): ("Child", "mozilla/media/MediaChild.h"),

    ("PPendingIPCBlob", "child"): ("PendingIPCBlobChild", "mozilla/dom/ipc/PendingIPCBlobChild.h"),
    ("PPendingIPCBlob", "parent"): (
        "mozilla::dom::PendingIPCBlobParent", "mozilla/dom/ipc/PendingIPCBlobParent.h"
    ),

    ("PPresentationRequest", "child"): (
        "PresentationRequestChild", "mozilla/dom/PresentationChild.h"
    ),
    ("PPresentationRequest", "parent"): (
        "PresentationRequestParent", "mozilla/dom/PresentationParent.h"
    ),

    ("PPrinting", "child"): ("nsPrintingProxy", "nsPrintingProxy.h"),
    ("PPrinting", "parent"): ("PrintingParent", "mozilla/embedding/printingui/PrintingParent.h"),

    ("PPSMContentDownloader", "child"): (
        "PSMContentDownloaderChild", "mozilla/psm/PSMContentListener.h"
    ),
    ("PPSMContentDownloader", "parent"): (
        "PSMContentDownloaderParent", "mozilla/psm/PSMContentListener.h"
    ),

    ("PRemoteSpellcheckEngine", "child"): (
        "RemoteSpellcheckEngineChild", "mozilla/RemoteSpellCheckEngineChild.h"
    ),
    ("PRemoteSpellcheckEngine", "parent"): (
        "RemoteSpellcheckEngineParent", "mozilla/RemoteSpellCheckEngineParent.h"
    ),

    ("PScriptCache", "child"): ("ScriptCacheChild", "mozilla/loader/ScriptCacheActors.h"),
    ("PScriptCache", "parent"): ("ScriptCacheParent", "mozilla/loader/ScriptCacheActors.h"),

    ("PTCPServerSocket", "child"): (
        "mozilla::dom::TCPServerSocketChild", "mozilla/dom/network/TCPServerSocketChild.h"
    ),
    ("PTCPServerSocket", "parent"): (
        "mozilla::dom::TCPServerSocketParent", "mozilla/dom/network/TCPServerSocketParent.h"
    ),
    ("PTCPSocket", "child"): (
        "mozilla::dom::TCPSocketChild", "mozilla/dom/network/TCPSocketChild.h"
    ),
    ("PTCPSocket", "parent"): (
        "mozilla::dom::TCPSocketParent", "mozilla/dom/network/TCPSocketParent.h"
    ),

    ("PTemporaryIPCBlob", "child"): (
        "mozilla::dom::TemporaryIPCBlobChild", "mozilla/dom/ipc/TemporaryIPCBlobChild.h"
    ),
    ("PTemporaryIPCBlob", "parent"): (
        "mozilla::dom::TemporaryIPCBlobParent", "mozilla/dom/ipc/TemporaryIPCBlobParent.h"
    ),

    ("PTestShellCommand", "parent"): ("TestShellCommandParent", "mozilla/ipc/TestShellParent.h"),

    ("PTransportProvider", "child"): (
        "TransportProviderChild", "mozilla/net/IPCTransportProvider.h"
    ),
    ("PTransportProvider", "parent"): (
        "TransportProviderParent", "mozilla/net/IPCTransportProvider.h"
    ),

    ("PUDPSocket", "child"): (
        "mozilla::dom::UDPSocketChild", "mozilla/dom/network/UDPSocketChild.h"
    ),
    ("PUDPSocket", "parent"): (
        "mozilla::dom::UDPSocketParent", "mozilla/dom/network/UDPSocketParent.h"
    ),

    ("PURLClassifierLocal", "child"): (
        "URLClassifierLocalChild", "mozilla/dom/URLClassifierChild.h"
    ),
    ("PURLClassifierLocal", "parent"): (
        "URLClassifierLocalParent", "mozilla/dom/URLClassifierParent.h"
    ),

    ("PVR", "child"): ("VRChild", "VRChild.h"),
    ("PVR", "parent"): ("VRParent", "VRParent.h"),
    ("PVRGPU", "child"): ("VRGPUChild", "VRGPUChild.h"),
    ("PVRGPU", "parent"): ("VRGPUParent", "VRGPUParent.h"),
    ("PVRLayer", "child"): ("VRLayerChild", "VRLayerChild.h"),
    ("PVRManager", "child"): ("VRManagerChild", "VRManagerChild.h"),
    ("PVRManager", "parent"): ("VRManagerParent", "VRManagerParent.h"),

    ("PWebSocket", "child"): ("WebSocketChannelChild", "mozilla/net/WebSocketChannelChild.h"),
    ("PWebSocket", "parent"): ("WebSocketChannelParent", "mozilla/net/WebSocketChannelParent.h"),
}

# Our long term goal is to burn this list down, so new entries should be added
# extremely sparingly and only with a very good reason! You must have an IPC
# peer's r+ to add something new!

# set() of (Protocol, side)
VIRTUAL_CALL_CLASSES = set([
    # Defined as a strange template
    ("PJavaScript", "child"),
    ("PJavaScript", "parent"),
    ("PMedia", "parent"),
    ("PTexture", "parent"),

    # Defined in a .cpp
    ("PAsmJSCacheEntry", "child"),
    ("PAsmJSCacheEntry", "parent"),
    ("PBackgroundFileHandle", "parent"),
    ("PBackgroundFileRequest", "parent"),
    ("PBackgroundIDBCursor", "parent"),
    ("PBackgroundIDBDatabase", "parent"),
    ("PBackgroundIDBDatabaseFile", "child"),
    ("PBackgroundIDBDatabaseFile", "parent"),
    ("PBackgroundIDBDatabaseRequest", "parent"),
    ("PBackgroundIDBFactory", "parent"),
    ("PBackgroundIDBFactoryRequest", "parent"),
    ("PBackgroundIDBRequest", "parent"),
    ("PBackgroundIDBTransaction", "parent"),
    ("PBackgroundIDBVersionChangeTransaction", "parent"),
    ("PBackgroundIndexedDBUtils", "parent"),
    ("PBackgroundLSDatabase", "parent"),
    ("PBackgroundLSObserver", "parent"),
    ("PBackgroundLSRequest", "parent"),
    ("PBackgroundLSSimpleRequest", "parent"),
    ("PBackgroundLSSnapshot", "parent"),
    ("PBackgroundSDBConnection", "parent"),
    ("PBackgroundSDBRequest", "parent"),
    ("PBackgroundTest", "child"),
    ("PBackgroundTest", "parent"),
    ("PChildToParentStream", "child"),
    ("PChildToParentStream", "parent"),
    ("PContentPermissionRequest", "parent"),
    ("PCycleCollectWithLogs", "child"),
    ("PCycleCollectWithLogs", "parent"),
    ("PHal", "child"),
    ("PHal", "parent"),
    ("PIndexedDBPermissionRequest", "parent"),
    ("PParentToChildStream", "child"),
    ("PParentToChildStream", "parent"),
    ("PProcessHangMonitor", "child"),
    ("PProcessHangMonitor", "parent"),
    ("PQuota", "parent"),
    ("PQuotaRequest", "parent"),
    ("PQuotaUsageRequest", "parent"),
    ("PSimpleChannel", "child"),
    ("PTexture", "child"),

    # .h is not exported
    ("PBackground", "child"),
    ("PBackground", "parent"),
    ("PBackgroundFileHandle", "child"),
    ("PBackgroundFileRequest", "child"),
    ("PBackgroundIDBCursor", "child"),
    ("PBackgroundIDBDatabase", "child"),
    ("PBackgroundIDBDatabaseRequest", "child"),
    ("PBackgroundIDBFactory", "child"),
    ("PBackgroundIDBFactoryRequest", "child"),
    ("PBackgroundIDBRequest", "child"),
    ("PBackgroundIDBTransaction", "child"),
    ("PBackgroundIDBVersionChangeTransaction", "child"),
    ("PBackgroundIndexedDBUtils", "child"),
    ("PBackgroundLSDatabase", "child"),
    ("PBackgroundLSObserver", "child"),
    ("PBackgroundLSRequest", "child"),
    ("PBackgroundLSSimpleRequest", "child"),
    ("PBackgroundLSSnapshot", "child"),
    ("PBackgroundMutableFile", "child"),
    ("PBackgroundSDBConnection", "child"),
    ("PBackgroundSDBRequest", "child"),
    ("PBroadcastChannel", "child"),
    ("PBroadcastChannel", "parent"),
    ("PChromiumCDM", "child"),
    ("PClientHandle", "child"),
    ("PClientHandle", "parent"),
    ("PClientHandleOp", "child"),
    ("PClientHandleOp", "parent"),
    ("PClientManager", "child"),
    ("PClientManager", "parent"),
    ("PClientManagerOp", "child"),
    ("PClientManagerOp", "parent"),
    ("PClientNavigateOp", "child"),
    ("PClientNavigateOp", "parent"),
    ("PClientOpenWindowOp", "child"),
    ("PClientOpenWindowOp", "parent"),
    ("PClientSource", "child"),
    ("PClientSource", "parent"),
    ("PClientSourceOp", "child"),
    ("PClientSourceOp", "parent"),
    ("PColorPicker", "child"),
    ("PColorPicker", "parent"),
    ("PDataChannel", "child"),
    ("PFileChannel", "child"),
    ("PFilePicker", "child"),
    ("PFunctionBroker", "child"),
    ("PFunctionBroker", "parent"),
    ("PHandlerService", "child"),
    ("PHandlerService", "parent"),
    ("PPluginBackgroundDestroyer", "child"),
    ("PPluginBackgroundDestroyer", "parent"),
    ("PPrintProgressDialog", "child"),
    ("PPrintProgressDialog", "parent"),
    ("PPrintSettingsDialog", "child"),
    ("PPrintSettingsDialog", "parent"),
    ("PQuota", "child"),
    ("PQuotaRequest", "child"),
    ("PQuotaUsageRequest", "child"),
    ("PServiceWorker", "child"),
    ("PServiceWorker", "parent"),
    ("PServiceWorkerContainer", "child"),
    ("PServiceWorkerContainer", "parent"),
    ("PServiceWorkerRegistration", "child"),
    ("PServiceWorkerRegistration", "parent"),
    ("PServiceWorkerUpdater", "child"),
    ("PServiceWorkerUpdater", "parent"),
    ("PVRLayer", "parent"),
    ("PWebBrowserPersistResources", "child"),
    ("PWebBrowserPersistResources", "parent"),
    ("PWebBrowserPersistSerialize", "child"),
    ("PWebBrowserPersistSerialize", "parent"),
    ("PWebrtcGlobal", "child"),
    ("PWebrtcGlobal", "parent"),

    # .h is only exported on some platforms/configs
    ("PCameras", "child"),
    ("PCameras", "parent"),
    ("PCompositorWidget", "child"),
    ("PCompositorWidget", "parent"),
    ("PDocAccessible", "child"),
    ("PDocAccessible", "parent"),
    ("PPluginSurface", "parent"),
    ("PPluginWidget", "child"),
    ("PPluginWidget", "parent"),
    ("PProfiler", "child"),
    ("PProfiler", "parent"),
    ("PSpeechSynthesisRequest", "child"),
    ("PSpeechSynthesisRequest", "parent"),
    ("PStunAddrsRequest", "child"),
    ("PStunAddrsRequest", "parent"),
    ("PWebrtcProxyChannel", "child"),
    ("PWebrtcProxyChannel", "parent"),

    # .h includes something that's a LOCAL_INCLUDE
    ("PBackgroundLocalStorageCache", "child"),
    ("PBackgroundLocalStorageCache", "parent"),
    ("PBackgroundStorage", "child"),
    ("PBackgroundStorage", "parent"),
    ("PBrowserStream", "parent"),
    ("PExternalHelperApp", "parent"),
    ("PFTPChannel", "child"),
    ("PFTPChannel", "parent"),
    ("PHttpChannel", "child"),
    ("PHttpChannel", "parent"),
    ("PSessionStorageObserver", "child"),
    ("PSessionStorageObserver", "parent"),
    ("PWyciwygChannel", "child"),

    # bug 1513911
    ("PIndexedDBPermissionRequest", "child"),

    # Recv* methods are MOZ_CAN_RUN_SCRIPT and OnMessageReceived is not, so
    # it's not allowed to call them.
    ("PBrowser", "child"),

    # can't be included safely for compilation error reasons
    ("PGMPContent", "parent"),
    ("PGMPService", "child"),
    ("PGMPService", "parent"),
    ("PGMPStorage", "parent"),
    ("PGMPVideoDecoder", "parent"),
    ("PGMPVideoEncoder", "parent"),
    ("PWebRenderBridge", "parent"),

    # Not actually subclassed
    ("PLoginReputation", "child"),
    ("PPluginSurface", "child"),
    ("PTestShellCommand", "child"),
])
