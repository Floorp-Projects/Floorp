This system add-on is a follow-up to the MITM prevalence experiment. The purpose
is to facilitate rolling out the disabling of SHA-1 in signatures on
certificates issued by publicly-trusted roots. When installed, this add-on will
perform a number of checks to determine if it should change the preference that
controls the SHA-1 policy. First, this should only apply to users on the beta
update channel. It should also only apply to users who have not otherwise
changed the policy to always allow or always forbid SHA-1. Additionally, it
must double-check that the user is not affected by a TLS intercepting proxy
using a publicly-trusted root. If these checks pass, the add-on will divide the
population into a test group and a control group (starting on a 10%/90% split).
The test group will have the policy changed. After doing this, a telemetry
payload is reported with the following values:

* cohortName -- the name of the group this user is in:
  1. "notSafeToDisableSHA1" if the user is behind a MITM proxy using a
     publicly-trusted root
  2. "optedOut" if the user already set the SHA-1 policy to always allow or
     always forbid
  3. "optedIn" if the user already set the SHA-1 policy to only allow for
     non-built-in roots
  4. "test" if the user is in the test cohort (and SHA-1 will be disabled)
  5. "control" if the user is not in the test cohort
* errorCode -- 0 for successful connections, some PR error code otherwise
* error -- a short description of one of four error conditions encountered, if
  applicable, and an empty string otherwise:
  1. "timeout" if the connection to telemetry.mozilla.org timed out
  2. "user override" if the user has stored a permanent certificate exception
     override for telemetry.mozilla.org (due to technical limitations, we can't
     gather much information in this situation)
  3. "certificate reverification" if re-building the certificate chain after
     connecting failed for some reason (unfortunately this step is necessary
     due to technical limitations)
  4. "connection error" if the connection to telemetry.mozilla.org failed for
     another reason
* chain -- a list of dictionaries each corresponding to a certificate in the
  verified certificate chain, if it was successfully constructed. The first
  entry is the end-entity certificate. The last entry is the root certificate.
  This will be empty if the connection failed or if reverification failed. Each
  element in the list contains the following values:
  * sha256Fingerprint -- a hex string representing the SHA-256 hash of the
    certificate
  * isBuiltInRoot -- true if the certificate is a trust anchor in the web PKI,
    false otherwise
  * signatureAlgorithm -- a description of the algorithm used to sign the
    certificate. Will be one of "md2WithRSAEncryption", "md5WithRSAEncryption",
    "sha1WithRSAEncryption", "sha256WithRSAEncryption",
    "sha384WithRSAEncryption", "sha512WithRSAEncryption", "ecdsaWithSHA1",
    "ecdsaWithSHA224", "ecdsaWithSHA256", "ecdsaWithSHA384", "ecdsaWithSHA512",
    or "unknown".
* disabledSHA1 -- true if SHA-1 was disabled, false otherwise
* didNotDisableSHA1Because -- a short string describing why SHA-1 could not be
    disabled, if applicable. Reasons are limited to:
    1. "MITM" if the user is behind a TLS intercepting proxy using a
       publicly-trusted root
    2. "connection error" if there was an error connecting to
       telemetry.mozilla.org
    3. "code error" if some inconsistent state was detected, and it was
       determined that the experiment should not attempt to change the
       preference
    4. "preference:userReset" if the user reset the SHA-1 policy after it had
       been changed by this add-on
    5. "preference:allow" if the user had already configured Firefox to always
       accept SHA-1 signatures
    6. "preference:forbid" if the user had already configured Firefox to always
       forbid SHA-1 signatures

For a connection not intercepted by a TLS proxy and where the user is in the
test cohort, the expected result will be:

    { "cohortName": "test",
      "errorCode": 0,
      "error": "",
      "chain": [
        { "sha256Fingerprint": "197feaf3faa0f0ad637a89c97cb91336bfc114b6b3018203cbd9c3d10c7fa86c",
          "isBuiltInRoot": false,
          "signatureAlgorithm": "sha256WithRSAEncryption"
        },
        { "sha256Fingerprint": "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
          "isBuiltInRoot": false,
          "signatureAlgorithm": "sha256WithRSAEncryption"
        },
        { "sha256Fingerprint": "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161",
          "isBuiltInRoot": true,
          "signatureAlgorithm": "sha1WithRSAEncryption"
        }
      ],
      "disabledSHA1": true,
      "didNotDisableSHA1Because": ""
    }

When this result is encountered, the user's preferences are updated to disable
SHA-1 in signatures on certificates issued by publicly-trusted roots.
Similarly, if the user is behind a TLS intercepting proxy but the root
certificate is not part of the public web PKI, we can also disable SHA-1 in
signatures on certificates issued by publicly-trusted roots.

If the user has already indicated in their preferences that they will always
accept SHA-1 in signatures or that they will never accept SHA-1 in signatures,
then the preference is not changed.
