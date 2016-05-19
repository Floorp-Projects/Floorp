/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the state of a recording rec from start to finish for recording,
 * completed, and rec data.
 */

const { PerformanceFront } = require("devtools/server/actors/performance");

add_task(function* () {
  let browser = yield addTab(MAIN_DOMAIN + "doc_perf.html");
  let doc = browser.contentDocument;

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();

  let rec = yield front.startRecording({ withMarkers: true, withTicks: true, withMemory: true });
  ok(rec.isRecording(), "RecordingModel is recording when created");
  yield busyWait(100);
  yield waitUntil(() => rec.getMemory().length);
  ok(true, "RecordingModel populates memory while recording");
  yield waitUntil(() => rec.getTicks().length);
  ok(true, "RecordingModel populates ticks while recording");
  yield waitUntil(() => rec.getMarkers().length);
  ok(true, "RecordingModel populates markers while recording");

  ok(!rec.isCompleted(), "RecordingModel is not completed when still recording");

  let stopping = once(front, "recording-stopping");
  let stopped = once(front, "recording-stopped");
  front.stopRecording(rec);

  yield stopping;
  ok(!rec.isRecording(), "on 'recording-stopping', model is no longer recording");
  // This handler should be called BEFORE "recording-stopped" is called, as
  // there is some delay, but in the event where "recording-stopped" finishes
  // before we check here, ensure that we're atleast in the right state
  if (rec.getProfile()) {
    ok(rec.isCompleted(), "recording is completed once it has profile data");
  } else {
    ok(!rec.isCompleted(), "recording is not yet completed on 'recording-stopping'");
    ok(rec.isFinalizing(), "recording is considered finalizing between 'recording-stopping' and 'recording-stopped'");
  }

  yield stopped;
  ok(!rec.isRecording(), "on 'recording-stopped', model is still no longer recording");
  ok(rec.isCompleted(), "on 'recording-stopped', model is considered 'complete'");

  checkSystemInfo(rec, "Host");
  checkSystemInfo(rec, "Client");

  // Export and import a rec, and ensure it has the correct state.
  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  yield rec.exportRecording(file);

  let importedModel = yield front.importRecording(file);

  ok(importedModel.isCompleted(), "All imported recordings should be completed");
  ok(!importedModel.isRecording(), "All imported recordings should not be recording");
  ok(importedModel.isImported(), "All imported recordings should be considerd imported");

  checkSystemInfo(importedModel, "Host");
  checkSystemInfo(importedModel, "Client");

  yield front.destroy();
  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function checkSystemInfo(recording, type) {
  let data = recording[`get${type}SystemInfo`]();
  for (let field of ["appid", "apptype", "vendor", "name", "version"]) {
    ok(data[field], `get${type}SystemInfo() has ${field} property`);
  }
}
