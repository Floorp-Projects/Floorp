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
    peerIdentity: id1
  }]);
  test.pcLocal.setIdentityProvider('test1.example.com', 'idp.js');
  test.pcRemote.setIdentityProvider('test2.example.com', 'idp.js');
  test.chain.append([
    function PEER_IDENTITY_IS_SET_CORRECTLY(test) {
      // no need to wait to check identity in this case,
      // setRemoteDescription should wait for the IdP to complete
      function checkIdentity(pc, pfx, idp, name) {
        return pc.peerIdentity.then(peerInfo => {
          is(peerInfo.idp, idp, pfx + "IdP check");
          is(peerInfo.name, name + "@" + idp, pfx + "identity check");
        });
      }

      return Promise.all([
        checkIdentity(test.pcLocal._pc, "local: ", "test2.example.com", "someone"),
        checkIdentity(test.pcRemote._pc, "remote: ", "test1.example.com", "someone")
      ]);
    },

    function REMOTE_STREAMS_ARE_RESTRICTED(test) {
      var remoteStream = test.pcLocal._pc.getRemoteStreams()[0];
      return Promise.all([
        audioIsSilence(true, remoteStream),
        videoIsBlack(true, remoteStream)
      ]);
    }
  ]);
  test.run();
}
