// This function takes a sample-rate, and tests that audio flows correctly when
// the sampling-rate at which the MTG runs is not one of the sampling-rates that
// the MediaPipeline can work with.
// It is in a separate file because we have an MTG per document, and we want to
// test multiple sample-rates, so we include it in multiple HTML mochitest
// files.
async function test_peerconnection_audio_forced_sample_rate(forcedSampleRate) {
  await scriptsReady;
  await pushPrefs(["media.cubeb.force_sample_rate", forcedSampleRate]);
  await runNetworkTest(function (options) {
    const test = new PeerConnectionTest(options);
    const ac = new AudioContext();
    test.setMediaConstraints([{ audio: true }], []);
    test.chain.replace("PC_LOCAL_GUM", [
      function PC_LOCAL_WEBAUDIO_SOURCE(test) {
        const oscillator = ac.createOscillator();
        oscillator.type = "sine";
        oscillator.frequency.value = 700;
        oscillator.start();
        const dest = ac.createMediaStreamDestination();
        oscillator.connect(dest);
        test.pcLocal.attachLocalStream(dest.stream);
      },
    ]);
    test.chain.append([
      function CHECK_REMOTE_AUDIO_FLOW(test) {
        return test.pcRemote.checkReceivingToneFrom(ac, test.pcLocal);
      },
    ]);
    return test.run();
  });
}
