/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_exempt_xxx() {
  await setupPolicyEngineWithJson({
    policies: {
      ExemptDomainFileTypePairsFromFileTypeDownloadWarnings: [
        {
          file_extension: "jnlp",
          domains: ["example.com", "www.example.edu"],
        },
      ],
    },
  });
  equal(
    Services.policies.isExemptExecutableExtension(
      "https://www.example.edu",
      "jnlp"
    ),
    true
  );
  equal(
    Services.policies.isExemptExecutableExtension(
      "https://example.edu",
      "jnlp"
    ),
    false
  );
  equal(
    Services.policies.isExemptExecutableExtension(
      "https://example.com",
      "jnlp"
    ),
    true
  );
  equal(
    Services.policies.isExemptExecutableExtension(
      "https://www.example.com",
      "jnlp"
    ),
    true
  );
  equal(
    Services.policies.isExemptExecutableExtension(
      "https://wwwexample.com",
      "jnlp"
    ),
    false
  );
  equal(
    Services.policies.isExemptExecutableExtension(
      "https://www.example.org",
      "jnlp"
    ),
    false
  );
  equal(
    Services.policies.isExemptExecutableExtension(
      "https://www.example.edu",
      "exe"
    ),
    false
  );
});
