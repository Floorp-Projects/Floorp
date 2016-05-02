/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

let db;
let userAgentID = 'f5b47f8d-771f-4ea3-b999-91c135f8766d';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: userAgentID,
  });
  run_next_test();
}

function putRecord(channelID, scope, publicKey, privateKey, authSecret) {
  return db.put({
    channelID: channelID,
    pushEndpoint: 'https://example.org/push/' + channelID,
    scope: scope,
    pushCount: 0,
    lastPush: 0,
    originAttributes: '',
    quota: Infinity,
    systemRecord: true,
    p256dhPublicKey: ChromeUtils.base64URLDecode(publicKey, {
      padding: "reject",
    }),
    p256dhPrivateKey: privateKey,
    authenticationSecret: ChromeUtils.base64URLDecode(authSecret, {
      padding: "reject",
    }),
  });
}

let ackDone;
let server;
add_task(function* test_notification_ack_data_setup() {
  db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  yield putRecord(
    'subscription1',
    'https://example.com/page/1',
    'BPCd4gNQkjwRah61LpdALdzZKLLnU5UAwDztQ5_h0QsT26jk0IFbqcK6-JxhHAm-rsHEwy0CyVJjtnfOcqc1tgA',
    {
      crv: 'P-256',
      d: '1jUPhzVsRkzV0vIzwL4ZEsOlKdNOWm7TmaTfzitJkgM',
      ext: true,
      key_ops: ["deriveBits"],
      kty: "EC",
      x: '8J3iA1CSPBFqHrUul0At3NkosudTlQDAPO1Dn-HRCxM',
      y: '26jk0IFbqcK6-JxhHAm-rsHEwy0CyVJjtnfOcqc1tgA'
    },
    'c_sGN6uCv9Hu7JOQT34jAQ'
  );
  yield putRecord(
    'subscription2',
    'https://example.com/page/2',
    'BPnWyUo7yMnuMlyKtERuLfWE8a09dtdjHSW2lpC9_BqR5TZ1rK8Ldih6ljyxVwnBA-nygQHGRpEmu1jV5K8437E',
    {
      crv: 'P-256',
      d: 'lFm4nPsUKYgNGBJb5nXXKxl8bspCSp0bAhCYxbveqT4',
      ext: true,
      key_ops: ["deriveBits"],
      kty: 'EC',
      x: '-dbJSjvIye4yXIq0RG4t9YTxrT1212MdJbaWkL38GpE',
      y: '5TZ1rK8Ldih6ljyxVwnBA-nygQHGRpEmu1jV5K8437E'
    },
    't3P246Gj9vjKDHHRYaY6hw'
  );
  yield putRecord(
    'subscription3',
    'https://example.com/page/3',
    'BDhUHITSeVrWYybFnb7ylVTCDDLPdQWMpf8gXhcWwvaaJa6n3YH8TOcH8narDF6t8mKVvg2ioLW-8MH5O4dzGcI',
    {
      crv: 'P-256',
      d: 'Q1_SE1NySTYzjbqgWwPgrYh7XRg3adqZLkQPsy319G8',
      ext: true,
      key_ops: ["deriveBits"],
      kty: 'EC',
      x: 'OFQchNJ5WtZjJsWdvvKVVMIMMs91BYyl_yBeFxbC9po',
      y: 'Ja6n3YH8TOcH8narDF6t8mKVvg2ioLW-8MH5O4dzGcI'
    },
    'E0qiXGWvFSR0PS352ES1_Q'
  );

  let setupDone;
  let setupDonePromise = new Promise(r => setupDone = r);

  PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          equal(request.uaid, userAgentID,
                'Should send matching device IDs in handshake');
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            uaid: userAgentID,
            status: 200,
            use_webpush: true,
          }));
          server = this;
          setupDone();
        },
        onACK(request) {
          if (ackDone) {
            ackDone(request);
          }
        }
      });
    }
  });
  yield setupDonePromise;
});

add_task(function* test_notification_ack_data() {
  let allTestData = [
    {
      channelID: 'subscription1',
      version: 'v1',
      send: {
        headers: {
          encryption_key: 'keyid="notification1"; dh="BO_tgGm-yvYAGLeRe16AvhzaUcpYRiqgsGOlXpt0DRWDRGGdzVLGlEVJMygqAUECarLnxCiAOHTP_znkedrlWoU"',
          encryption: 'keyid="notification1";salt="uAZaiXpOSfOLJxtOCZ09dA"',
          encoding: 'aesgcm128',
        },
        data: 'NwrrOWPxLE8Sv5Rr0Kep7n0-r_j3rsYrUw_CXPo',
        version: 'v1',
      },
      ackCode: 100,
      receive: {
        scope: 'https://example.com/page/1',
        data: 'Some message'
      }
    },
    {
      channelID: 'subscription2',
      version: 'v2',
      send: {
        headers: {
          encryption_key: 'keyid="notification2"; dh="BKVdQcgfncpNyNWsGrbecX0zq3eHIlHu5XbCGmVcxPnRSbhjrA6GyBIeGdqsUL69j5Z2CvbZd-9z1UBH0akUnGQ"',
          encryption: 'keyid="notification2";salt="vFn3t3M_k42zHBdpch3VRw"',
          encoding: 'aesgcm128',
        },
        data: 'Zt9dEdqgHlyAL_l83385aEtb98ZBilz5tgnGgmwEsl5AOCNgesUUJ4p9qUU',
      },
      ackCode: 100,
      receive: {
        scope: 'https://example.com/page/2',
        data: 'Some message'
      }
    },
    {
      channelID: 'subscription3',
      version: 'v3',
      send: {
        headers: {
          encryption_key: 'keyid="notification3";dh="BD3xV_ACT8r6hdIYES3BJj1qhz9wyv7MBrG9vM2UCnjPzwE_YFVpkD-SGqE-BR2--0M-Yf31wctwNsO1qjBUeMg"',
          encryption: 'keyid="notification3"; salt="DFq188piWU7osPBgqn4Nlg"; rs=24',
          encoding: 'aesgcm128',
        },
        data: 'LKru3ZzxBZuAxYtsaCfaj_fehkrIvqbVd1iSwnwAUgnL-cTeDD-83blxHXTq7r0z9ydTdMtC3UjAcWi8LMnfY-BFzi0qJAjGYIikDA',
      },
      ackCode: 100,
      receive: {
        scope: 'https://example.com/page/3',
        data: 'Some message'
      }
    },
    // A message encoded with `aesgcm` (2 bytes of padding, authenticated).
    {
      channelID: 'subscription1',
      version: 'v5',
      send: {
        headers: {
          crypto_key: 'keyid=v4;dh="BMh_vsnqu79ZZkMTYkxl4gWDLdPSGE72Lr4w2hksSFW398xCMJszjzdblAWXyhSwakRNEU_GopAm4UGzyMVR83w"',
          encryption: 'keyid="v4";salt="C14Wb7rQTlXzrgcPHtaUzw"',
          encoding: 'aesgcm',
        },
        data: 'pus4kUaBWzraH34M-d_oN8e0LPpF_X6acx695AMXovDe',
      },
      ackCode: 100,
      receive: {
        scope: 'https://example.com/page/1',
        data: 'Another message'
      }
    },
    // A message with 17 bytes of padding and rs of 24
    {
      channelID: 'subscription2',
      version: 'v5',
      send: {
        headers: {
          crypto_key: 'keyid="v5"; dh="BOp-DpyR9eLY5Ci11_loIFqeHzWfc_0evJmq7N8NKzgp60UAMMM06XIi2VZp2_TSdw1omk7E19SyeCCwRp76E-U"',
          encryption: 'keyid=v5;salt="TvjOou1TqJOQY_ZsOYV3Ww";rs=24',
          encoding: 'aesgcm',
        },
        data: 'rG9WYQ2ZwUgfj_tMlZ0vcIaNpBN05FW-9RUBZAM-UUZf0_9eGpuENBpUDAw3mFmd2XJpmvPvAtLVs54l3rGwg1o',
      },
      ackCode: 100,
      receive: {
        scope: 'https://example.com/page/2',
        data: 'Some message'
      }
    },
    // A message without key identifiers.
    {
      channelID: 'subscription3',
      version: 'v6',
      send: {
        headers: {
          crypto_key: 'dh="BEEjwWbF5jZKCgW0kmUWgG-wNcRvaa9_3zZElHAF8przHwd4cp5_kQsc-IMNZcVA0iUix31jxuMOytU-5DwWtyQ"',
          encryption: 'salt=aAQcr2khAksgNspPiFEqiQ',
          encoding: 'aesgcm',
        },
        data: 'pEYgefdI-7L46CYn5dR9TIy2AXGxe07zxclbhstY',
      },
      ackCode: 100,
      receive: {
        scope: 'https://example.com/page/3',
        data: 'Some message'
      }
    },
    // A malformed encrypted message.
    {
      channelID: 'subscription3',
      version: 'v7',
      send: {
        headers: {
          crypto_key: 'dh=AAAAAAAA',
          encryption: 'salt=AAAAAAAA',
        },
        data: 'AAAAAAAA',
      },
      ackCode: 101,
      receive: null,
    },
  ];

  let sendAndReceive = testData => {
    let messageReceived = testData.receive ? promiseObserverNotification(PushServiceComponent.pushTopic, (subject, data) => {
      let notification = subject.QueryInterface(Ci.nsIPushMessage).data;
      equal(notification.text(), testData.receive.data,
            'Check data for notification ' + testData.version);
      equal(data, testData.receive.scope,
            'Check scope for notification ' + testData.version);
      return true;
    }) : Promise.resolve();

    let ackReceived = new Promise(resolve => ackDone = resolve)
        .then(ackData => {
          deepEqual({
            messageType: 'ack',
            updates: [{
              channelID: testData.channelID,
              version: testData.version,
              code: testData.ackCode,
            }],
          }, ackData, 'Check updates for acknowledgment ' + testData.version);
        });

    let msg = JSON.parse(JSON.stringify(testData.send));
    msg.messageType = 'notification';
    msg.channelID = testData.channelID;
    msg.version = testData.version;
    server.serverSendMsg(JSON.stringify(msg));

    return Promise.all([messageReceived, ackReceived]);
  };

  yield allTestData.reduce((p, testData) => {
    return p.then(_ => sendAndReceive(testData));
  }, Promise.resolve());
});
