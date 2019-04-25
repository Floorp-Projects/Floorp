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

    ("PLoginReputation", "parent"): ("LoginReputationParent", "mozilla/LoginReputationIPC.h"),

    ("PMedia", "child"): ("Child", "mozilla/media/MediaChild.h"),

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

    # _ipdltest
    ("PTestActorPunning", "child"): (
        "TestActorPunningChild", "mozilla/_ipdltest/TestActorPunning.h"
    ),
    ("PTestActorPunning", "parent"): (
        "TestActorPunningParent", "mozilla/_ipdltest/TestActorPunning.h"
    ),
    ("PTestActorPunningPunned", "child"): (
        "TestActorPunningPunnedChild", "mozilla/_ipdltest/TestActorPunning.h"
    ),
    ("PTestActorPunningPunned", "parent"): (
        "TestActorPunningPunnedParent", "mozilla/_ipdltest/TestActorPunning.h"
    ),
    ("PTestActorPunningSub", "child"): (
        "TestActorPunningSubChild", "mozilla/_ipdltest/TestActorPunning.h"
    ),
    ("PTestActorPunningSub", "parent"): (
        "TestActorPunningSubParent", "mozilla/_ipdltest/TestActorPunning.h"
    ),

    ("PTestAsyncReturns", "child"): (
        "TestAsyncReturnsChild", "mozilla/_ipdltest/TestAsyncReturns.h"
    ),
    ("PTestAsyncReturns", "parent"): (
        "TestAsyncReturnsParent", "mozilla/_ipdltest/TestAsyncReturns.h"
    ),

    ("PTestBadActor", "parent"): ("TestBadActorParent", "mozilla/_ipdltest/TestBadActor.h"),
    ("PTestBadActor", "child"): ("TestBadActorChild", "mozilla/_ipdltest/TestBadActor.h"),
    ("PTestBadActorSub", "child"): ("TestBadActorSubChild", "mozilla/_ipdltest/TestBadActor.h"),
    ("PTestBadActorSub", "parent"): ("TestBadActorSubParent", "mozilla/_ipdltest/TestBadActor.h"),

    ("PTestCancel", "child"): ("TestCancelChild", "mozilla/_ipdltest/TestCancel.h"),
    ("PTestCancel", "parent"): ("TestCancelParent", "mozilla/_ipdltest/TestCancel.h"),

    ("PTestCrashCleanup", "child"): (
        "TestCrashCleanupChild", "mozilla/_ipdltest/TestCrashCleanup.h"
    ),
    ("PTestCrashCleanup", "parent"): (
        "TestCrashCleanupParent", "mozilla/_ipdltest/TestCrashCleanup.h"
    ),

    ("PTestDataStructures", "child"): (
        "TestDataStructuresChild", "mozilla/_ipdltest/TestDataStructures.h"
    ),
    ("PTestDataStructures", "parent"): (
        "TestDataStructuresParent", "mozilla/_ipdltest/TestDataStructures.h"
    ),
    ("PTestDataStructuresSub", "child"): (
        "TestDataStructuresSub", "mozilla/_ipdltest/TestDataStructures.h"
    ),
    ("PTestDataStructuresSub", "parent"): (
        "TestDataStructuresSub", "mozilla/_ipdltest/TestDataStructures.h"
    ),

    ("PTestDemon", "child"): ("TestDemonChild", "mozilla/_ipdltest/TestDemon.h"),
    ("PTestDemon", "parent"): ("TestDemonParent", "mozilla/_ipdltest/TestDemon.h"),

    ("PTestDesc", "child"): ("TestDescChild", "mozilla/_ipdltest/TestDesc.h"),
    ("PTestDesc", "parent"): ("TestDescParent", "mozilla/_ipdltest/TestDesc.h"),
    ("PTestDescSub", "child"): ("TestDescSubChild", "mozilla/_ipdltest/TestDesc.h"),
    ("PTestDescSub", "parent"): ("TestDescSubParent", "mozilla/_ipdltest/TestDesc.h"),
    ("PTestDescSubsub", "child"): ("TestDescSubsubChild", "mozilla/_ipdltest/TestDesc.h"),
    ("PTestDescSubsub", "parent"): ("TestDescSubsubParent", "mozilla/_ipdltest/TestDesc.h"),

    ("PTestEndpointBridgeMain", "child"): (
        "TestEndpointBridgeMainChild", "mozilla/_ipdltest/TestEndpointBridgeMain.h"
    ),
    ("PTestEndpointBridgeMain", "parent"): (
        "TestEndpointBridgeMainParent", "mozilla/_ipdltest/TestEndpointBridgeMain.h"
    ),
    ("PTestEndpointBridgeMainSub", "child"): (
        "TestEndpointBridgeMainSubChild", "mozilla/_ipdltest/TestEndpointBridgeMain.h"
    ),
    ("PTestEndpointBridgeMainSub", "parent"): (
        "TestEndpointBridgeMainSubParent", "mozilla/_ipdltest/TestEndpointBridgeMain.h"
    ),
    ("PTestEndpointBridgeSub", "child"): (
        "TestEndpointBridgeSubChild", "mozilla/_ipdltest/TestEndpointBridgeMain.h"
    ),
    ("PTestEndpointBridgeSub", "parent"): (
        "TestEndpointBridgeSubParent", "mozilla/_ipdltest/TestEndpointBridgeMain.h"
    ),

    ("PTestEndpointOpens", "child"): (
        "TestEndpointOpensChild", "mozilla/_ipdltest/TestEndpointOpens.h"
    ),
    ("PTestEndpointOpens", "parent"): (
        "TestEndpointOpensParent", "mozilla/_ipdltest/TestEndpointOpens.h"
    ),
    ("PTestEndpointOpensOpened", "child"): (
        "TestEndpointOpensOpenedChild", "mozilla/_ipdltest/TestEndpointOpens.h"
    ),
    ("PTestEndpointOpensOpened", "parent"): (
        "TestEndpointOpensOpenedParent", "mozilla/_ipdltest/TestEndpointOpens.h"
    ),

    ("PTestFailedCtor", "child"): ("TestFailedCtorChild", "mozilla/_ipdltest/TestFailedCtor.h"),
    ("PTestFailedCtor", "parent"): ("TestFailedCtorParent", "mozilla/_ipdltest/TestFailedCtor.h"),
    ("PTestFailedCtorSub", "child"): (
        "TestFailedCtorSubChild", "mozilla/_ipdltest/TestFailedCtor.h"
    ),
    ("PTestFailedCtorSub", "parent"): (
        "TestFailedCtorSubParent", "mozilla/_ipdltest/TestFailedCtor.h"
    ),
    ("PTestFailedCtorSubsub", "child"): (
        "TestFailedCtorSubsub", "mozilla/_ipdltest/TestFailedCtor.h"
    ),
    ("PTestFailedCtorSubsub", "parent"): (
        "TestFailedCtorSubsub", "mozilla/_ipdltest/TestFailedCtor.h"
    ),

    ("PTestHandle", "child"): ("TestHandleChild", "mozilla/_ipdltest/TestJSON.h"),
    ("PTestHandle", "parent"): ("TestHandleParent", "mozilla/_ipdltest/TestJSON.h"),
    ("PTestJSON", "child"): ("TestJSONChild", "mozilla/_ipdltest/TestJSON.h"),
    ("PTestJSON", "parent"): ("TestJSONParent", "mozilla/_ipdltest/TestJSON.h"),

    ("PTestHangs", "child"): ("TestHangsChild", "mozilla/_ipdltest/TestHangs.h"),
    ("PTestHangs", "parent"): ("TestHangsParent", "mozilla/_ipdltest/TestHangs.h"),

    ("PTestHighestPrio", "child"): ("TestHighestPrioChild", "mozilla/_ipdltest/TestHighestPrio.h"),
    ("PTestHighestPrio", "parent"): (
        "TestHighestPrioParent", "mozilla/_ipdltest/TestHighestPrio.h"
    ),

    ("PTestInterruptErrorCleanup", "child"): (
        "TestInterruptErrorCleanupChild", "mozilla/_ipdltest/TestInterruptErrorCleanup.h"
    ),
    ("PTestInterruptErrorCleanup", "parent"): (
        "TestInterruptErrorCleanupParent", "mozilla/_ipdltest/TestInterruptErrorCleanup.h"
    ),

    ("PTestInterruptRaces", "child"): (
        "TestInterruptRacesChild", "mozilla/_ipdltest/TestInterruptRaces.h"
    ),
    ("PTestInterruptRaces", "parent"): (
        "TestInterruptRacesParent", "mozilla/_ipdltest/TestInterruptRaces.h"
    ),

    ("PTestInterruptShutdownRace", "child"): (
        "TestInterruptShutdownRaceChild", "mozilla/_ipdltest/TestInterruptShutdownRace.h"
    ),
    ("PTestInterruptShutdownRace", "parent"): (
        "TestInterruptShutdownRaceParent", "mozilla/_ipdltest/TestInterruptShutdownRace.h"
    ),

    ("PTestLatency", "child"): ("TestLatencyChild", "mozilla/_ipdltest/TestLatency.h"),
    ("PTestLatency", "parent"): ("TestLatencyParent", "mozilla/_ipdltest/TestLatency.h"),

    ("PTestLayoutThread", "child"): (
        "TestOffMainThreadPaintingChild", "mozilla/_ipdltest/TestOffMainThreadPainting.h"
    ),
    ("PTestLayoutThread", "parent"): (
        "TestOffMainThreadPaintingParent", "mozilla/_ipdltest/TestOffMainThreadPainting.h"
    ),
    ("PTestPaintThread", "child"): (
        "TestPaintThreadChild", "mozilla/_ipdltest/TestOffMainThreadPainting.h"
    ),
    ("PTestPaintThread", "parent"): (
        "TestPaintThreadParent", "mozilla/_ipdltest/TestOffMainThreadPainting.h"
    ),

    ("PTestManyChildAllocs", "child"): (
        "TestManyChildAllocsChild", "mozilla/_ipdltest/TestManyChildAllocs.h"
    ),
    ("PTestManyChildAllocs", "parent"): (
        "TestManyChildAllocsParent", "mozilla/_ipdltest/TestManyChildAllocs.h"
    ),
    ("PTestManyChildAllocsSub", "child"): (
        "TestManyChildAllocsSubChild", "mozilla/_ipdltest/TestManyChildAllocs.h"
    ),
    ("PTestManyChildAllocsSub", "parent"): (
        "TestManyChildAllocsSubParent", "mozilla/_ipdltest/TestManyChildAllocs.h"
    ),

    ("PTestMultiMgrs", "child"): ("TestMultiMgrsChild", "mozilla/_ipdltest/TestMultiMgrs.h"),
    ("PTestMultiMgrs", "parent"): ("TestMultiMgrsParent", "mozilla/_ipdltest/TestMultiMgrs.h"),
    ("PTestMultiMgrsBottom", "child"): (
        "TestMultiMgrsBottomChild", "mozilla/_ipdltest/TestMultiMgrs.h"
    ),
    ("PTestMultiMgrsBottom", "parent"): (
        "TestMultiMgrsBottomParent", "mozilla/_ipdltest/TestMultiMgrs.h"
    ),
    ("PTestMultiMgrsLeft", "child"): (
        "TestMultiMgrsLeftChild", "mozilla/_ipdltest/TestMultiMgrs.h"
    ),
    ("PTestMultiMgrsLeft", "parent"): (
        "TestMultiMgrsLeftParent", "mozilla/_ipdltest/TestMultiMgrs.h"
    ),
    ("PTestMultiMgrsRight", "child"): (
        "TestMultiMgrsRightChild", "mozilla/_ipdltest/TestMultiMgrs.h"
    ),
    ("PTestMultiMgrsRight", "parent"): (
        "TestMultiMgrsRightParent", "mozilla/_ipdltest/TestMultiMgrs.h"
    ),

    ("PTestNestedLoops", "child"): ("TestNestedLoopsChild", "mozilla/_ipdltest/TestNestedLoops.h"),
    ("PTestNestedLoops", "parent"): (
        "TestNestedLoopsParent", "mozilla/_ipdltest/TestNestedLoops.h"
    ),

    ("PTestRaceDeadlock", "child"): (
        "TestRaceDeadlockChild", "mozilla/_ipdltest/TestRaceDeadlock.h"
    ),
    ("PTestRaceDeadlock", "parent"): (
        "TestRaceDeadlockParent", "mozilla/_ipdltest/TestRaceDeadlock.h"
    ),

    ("PTestRaceDeferral", "child"): (
        "TestRaceDeferralChild", "mozilla/_ipdltest/TestRaceDeferral.h"
    ),
    ("PTestRaceDeferral", "parent"): (
        "TestRaceDeferralParent", "mozilla/_ipdltest/TestRaceDeferral.h"
    ),

    ("PTestRacyInterruptReplies", "child"): (
        "TestRacyInterruptRepliesChild", "mozilla/_ipdltest/TestRacyInterruptReplies.h"
    ),
    ("PTestRacyInterruptReplies", "parent"): (
        "TestRacyInterruptRepliesParent", "mozilla/_ipdltest/TestRacyInterruptReplies.h"
    ),

    ("PTestRacyReentry", "child"): ("TestRacyReentryChild", "mozilla/_ipdltest/TestRacyReentry.h"),
    ("PTestRacyReentry", "parent"): (
        "TestRacyReentryParent", "mozilla/_ipdltest/TestRacyReentry.h"
    ),

    ("PTestRacyUndefer", "child"): ("TestRacyUndeferChild", "mozilla/_ipdltest/TestRacyUndefer.h"),
    ("PTestRacyUndefer", "parent"): (
        "TestRacyUndeferParent", "mozilla/_ipdltest/TestRacyUndefer.h"
    ),

    ("PTestRPC", "child"): ("TestRPCChild", "mozilla/_ipdltest/TestRPC.h"),
    ("PTestRPC", "parent"): ("TestRPCParent", "mozilla/_ipdltest/TestRPC.h"),

    ("PTestSanity", "child"): ("TestSanityChild", "mozilla/_ipdltest/TestSanity.h"),
    ("PTestSanity", "parent"): ("TestSanityParent", "mozilla/_ipdltest/TestSanity.h"),

    ("PTestSelfManage", "child"): (
        "TestSelfManageChild", "mozilla/_ipdltest/TestSelfManageRoot.h"
    ),
    ("PTestSelfManage", "parent"): (
        "TestSelfManageParent", "mozilla/_ipdltest/TestSelfManageRoot.h"
    ),
    ("PTestSelfManageRoot", "child"): (
        "TestSelfManageRootChild", "mozilla/_ipdltest/TestSelfManageRoot.h"
    ),
    ("PTestSelfManageRoot", "parent"): (
        "TestSelfManageRootParent", "mozilla/_ipdltest/TestSelfManageRoot.h"
    ),

    ("PTestShmem", "child"): ("TestShmemChild", "mozilla/_ipdltest/TestShmem.h"),
    ("PTestShmem", "parent"): ("TestShmemParent", "mozilla/_ipdltest/TestShmem.h"),

    ("PTestShutdown", "child"): ("TestShutdownChild", "mozilla/_ipdltest/TestShutdown.h"),
    ("PTestShutdown", "parent"): ("TestShutdownParent", "mozilla/_ipdltest/TestShutdown.h"),
    ("PTestShutdownSub", "child"): ("TestShutdownSubChild", "mozilla/_ipdltest/TestShutdown.h"),
    ("PTestShutdownSub", "parent"): ("TestShutdownSubParent", "mozilla/_ipdltest/TestShutdown.h"),
    ("PTestShutdownSubsub", "child"): (
        "TestShutdownSubsubChild", "mozilla/_ipdltest/TestShutdown.h"
    ),
    ("PTestShutdownSubsub", "parent"): (
        "TestShutdownSubsubParent", "mozilla/_ipdltest/TestShutdown.h"
    ),

    ("PTestStackHooks", "child"): ("TestStackHooksChild", "mozilla/_ipdltest/TestStackHooks.h"),
    ("PTestStackHooks", "parent"): ("TestStackHooksParent", "mozilla/_ipdltest/TestStackHooks.h"),

    ("PTestSyncError", "child"): ("TestSyncErrorChild", "mozilla/_ipdltest/TestSyncError.h"),
    ("PTestSyncError", "parent"): ("TestSyncErrorParent", "mozilla/_ipdltest/TestSyncError.h"),

    ("PTestSyncHang", "child"): ("TestSyncHangChild", "mozilla/_ipdltest/TestSyncHang.h"),
    ("PTestSyncHang", "parent"): ("TestSyncHangParent", "mozilla/_ipdltest/TestSyncHang.h"),

    ("PTestSyncWakeup", "child"): ("TestSyncWakeupChild", "mozilla/_ipdltest/TestSyncWakeup.h"),
    ("PTestSyncWakeup", "parent"): ("TestSyncWakeupParent", "mozilla/_ipdltest/TestSyncWakeup.h"),

    ("PTestUniquePtrIPC", "child"): (
        "TestUniquePtrIPCChild", "mozilla/_ipdltest/TestUniquePtrIPC.h"
    ),
    ("PTestUniquePtrIPC", "parent"): (
        "TestUniquePtrIPCParent", "mozilla/_ipdltest/TestUniquePtrIPC.h"
    ),

    ("PTestUrgency", "child"): ("TestUrgencyChild", "mozilla/_ipdltest/TestUrgency.h"),
    ("PTestUrgency", "parent"): ("TestUrgencyParent", "mozilla/_ipdltest/TestUrgency.h"),

    ("PTestUrgentHangs", "child"): ("TestUrgentHangsChild", "mozilla/_ipdltest/TestUrgentHangs.h"),
    ("PTestUrgentHangs", "parent"): (
        "TestUrgentHangsParent", "mozilla/_ipdltest/TestUrgentHangs.h"
    ),
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

    # _ipdltest
    # Not actually subclassed
    ("PTestIndirectProtocolParamFirst", "child"),
    ("PTestIndirectProtocolParamFirst", "parent"),
    ("PTestIndirectProtocolParamManage", "child"),
    ("PTestIndirectProtocolParamManage", "parent"),
    ("PTestIndirectProtocolParamSecond", "child"),
    ("PTestIndirectProtocolParamSecond", "parent"),
    ("PTestPriority", "child"),
    ("PTestPriority", "parent"),
])
