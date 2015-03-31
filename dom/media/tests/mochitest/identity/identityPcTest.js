function identityPcTest(remoteOptions) {
  var user = 'someone';
  var domain1 = 'test1.example.com';
  var domain2 = 'test2.example.com';
  var id1 = user + '@' + domain1;
  var id2 = user + '@' + domain2;

  test = new PeerConnectionTest({
    config_local: {
      peerIdentity: id2
    },
    config_remote: {
      peerIdentity: id1
    }
  });
  test.setMediaConstraints([{
    audio: true,
    video: true,
    peerIdentity: id2
  }], [remoteOptions || {
    audio: true,
    video: true,
    fake: true,
    peerIdentity: id1
  }]);
  test.setIdentityProvider(test.pcLocal, 'test1.example.com', 'idp.html');
  test.setIdentityProvider(test.pcRemote, 'test2.example.com', 'idp.html');
  test.chain.append([

    function PEER_IDENTITY_IS_SET_CORRECTLY(test) {
      // no need to wait to check identity in this case,
      // setRemoteDescription should wait for the IdP to complete
      function checkIdentity(pc, pfx, idp, name) {
        is(pc.peerIdentity.idp, idp, pfx + "IdP is correct");
        is(pc.peerIdentity.name, name + "@" + idp, pfx + "identity is correct");
      }

      checkIdentity(test.pcLocal._pc, "local: ", "test2.example.com", "someone");
      checkIdentity(test.pcRemote._pc, "remote: ", "test1.example.com", "someone");
    },

    function REMOTE_STREAMS_ARE_RESTRICTED(test) {
      var remoteStream = test.pcLocal._pc.getRemoteStreams()[0];
      return Promise.all([
        new Promise(done => audioIsSilence(true, remoteStream, done)),
        new Promise(done => videoIsBlack(true, remoteStream, done))
      ]);
    }
  ]);
  test.run();
}
