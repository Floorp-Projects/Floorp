/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the state of a recording rec from start to finish for recording,
 * completed, and rec data.
 */

"use strict";

const { PerformanceFront } = require("devtools/shared/fronts/performance");

add_task(async function() {
  await addTab(MAIN_DOMAIN + "doc_perf.html");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const front = PerformanceFront(client, form);
  await front.connect();

  const rec = await front.startRecording(
    { withMarkers: true, withTicks: true, withMemory: true });
  ok(rec.isRecording(), "RecordingModel is recording when created");
  await busyWait(100);
  await waitUntil(() => rec.getMemory().length);
  ok(true, "RecordingModel populates memory while recording");
  await waitUntil(() => rec.getTicks().length);
  ok(true, "RecordingModel populates ticks while recording");
  await waitUntil(() => rec.getMarkers().length);
  ok(true, "RecordingModel populates markers while recording");

  ok(!rec.isCompleted(), "RecordingModel is not completed when still recording");

  const stopping = once(front, "recording-stopping");
  const stopped = once(front, "recording-stopped");
  front.stopRecording(rec);

  await stopping;
  ok(!rec.isRecording(), "on 'recording-stopping', model is no longer recording");
  // This handler should be called BEFORE "recording-stopped" is called, as
  // there is some delay, but in the event where "recording-stopped" finishes
  // before we check here, ensure that we're atleast in the right state
  if (rec.getProfile()) {
    ok(rec.isCompleted(), "recording is completed once it has profile data");
  } else {
    ok(!rec.isCompleted(), "recording is not yet completed on 'recording-stopping'");
    ok(rec.isFinalizing(),
      "recording is finalized between 'recording-stopping' and 'recording-stopped'");
  }

  await stopped;
  ok(!rec.isRecording(), "on 'recording-stopped', model is still no longer recording");
  ok(rec.isCompleted(), "on 'recording-stopped', model is considered 'complete'");

  checkSystemInfo(rec, "Host");
  checkSystemInfo(rec, "Client");

  // Export and import a rec, and ensure it has the correct state.
  const file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  await rec.exportRecording(file);

  const importedModel = await front.importRecording(file);

  ok(importedModel.isCompleted(), "All imported recordings should be completed");
  ok(!importedModel.isRecording(), "All imported recordings should not be recording");
  ok(importedModel.isImported(), "All imported recordings should be considerd imported");

  checkSystemInfo(importedModel, "Host");
  checkSystemInfo(importedModel, "Client");

  await front.destroy();
  await client.close();
  gBrowser.removeCurrentTab();
});

function checkSystemInfo(recording, type) {
  const data = recording[`get${type}SystemInfo`]();
  for (const field of ["appid", "apptype", "vendor", "name", "version"]) {
    ok(data[field], `get${type}SystemInfo() has ${field} property`);
  }
}
