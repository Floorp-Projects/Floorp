/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

// Binary form of test certificate.
var testCertInfo = {
  nickname: 'Test Certificate',
  password: '',
  usage: ['UserCert'],
  blob: '-----BEGIN CERTIFICATE-----\n' +
        'MIICTjCCAbegAwIBAgICNV4wDQYJKoZIhvcNAQEEBQAwgYUxCzAJBgNVBAYTAklU\n' +
        'MRYwFAYDVQQKEw1aZXJvc2hlbGwubmV0MRAwDgYDVQQLEwdFeGFtcGxlMR0wGwYD\n' +
        'VQQDExRaZXJvU2hlbGwgRXhhbXBsZSBDQTEtMCsGCSqGSIb3DQEJARYeRnVsdmlv\n' +
        'LlJpY2NpYXJkaUB6ZXJvc2hlbGwubmV0MB4XDTEzMDMxMTAzMzg1MloXDTE0MDMx\n' +
        'MTAzMzg1MlowIzEOMAwGA1UECxMFVXNlcnMxETAPBgNVBAMTCGNodWNrbGVlMIGf\n' +
        'MA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDvVzFhQAVqAIHW5DlAhp4FEGEei7k7\n' +
        'uVUeqkH7JAsww6zmDLg9yZlcZAc95N0lkz022gLXehH2M0R1FOR++nkqofzWfc7w\n' +
        'n79ith+dU2GQMeKq7vPGDYXpgIkEKbYfzKj3fY3129MlTxJQt1UD/ejz38V8HKgw\n' +
        'qKSuwo0NVeY66QIDAQABoy4wLDALBgNVHQ8EBAMCBLAwHQYDVR0lBBYwFAYIKwYB\n' +
        'BQUHAwIGCCsGAQUFBwMEMA0GCSqGSIb3DQEBBAUAA4GBAJWgfX5vYSD7MGZk1rTF\n' +
        'DSziWYGqpR+Moo3qQ+9qLG8m+XVM9hckWpY31A5sWAeCZCe1SSNLFbbgsaOyPZE2\n' +
        'NqMyvs61Vszpc2mmWAYT6j2OU2tw8p5pcUZd6eIp7Gc3fLymiX/WoSmilZKmrGUZ\n' +
        'Q15R+TCpclUsaNrUGjybgaw7\n' +
        '-----END CERTIFICATE-----'
};

gTestSuite.doTestWithCertificate(
  new Blob([testCertInfo.blob]),
  testCertInfo.password,
  testCertInfo.nickname,
  testCertInfo.usage
);
