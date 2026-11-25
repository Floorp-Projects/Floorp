// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  M3A_FIXTURE_ENVELOPE,
  M3A_FIXTURE_PUBLIC_KEY,
  M3A_INERT_PROBE_ENVELOPE,
  M3A_INERT_PROBE_PUBLIC_KEY,
} from "../package/FeaturePackageFixture.ts";
import type {
  FeaturePackagePolicy,
  VerifiedFeaturePackage,
} from "../package/types.ts";

type VerifierModule =
  typeof import("../package/FeaturePackageVerifier.sys.mts");
const SOURCE_PROBE_PREF = "nora.m3a.package_source_execution_probe";
const verifier = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/package/FeaturePackageVerifier.sys.mjs",
) as VerifierModule;

function assert(condition: unknown, message: string): asserts condition {
  harnessAssert(condition, message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  harnessAssertEquals(actual, expected, message);
}

async function expectCode(
  operation: Promise<unknown>,
  code: string,
): Promise<void> {
  let caught: unknown;
  try {
    await operation;
  } catch (error: unknown) {
    caught = error;
  }
  assert(
    caught instanceof verifier.FeaturePackageError,
    "bounded package error is returned",
  );
  assertEquals(caught.code, code, "package error code matches");
}

function fixturePolicy(): FeaturePackagePolicy {
  return {
    featureId: "hub-panel-menu",
    hostSchema: 2,
    hostBuildId: "fixture-host",
    runtimeBuildId: "fixture-runtime",
    allowedCapabilities: ["hub-panel"],
    minimumReleaseSequence: 1,
    trustedKeys: [{ keyId: "fixture-key", publicKey: M3A_FIXTURE_PUBLIC_KEY }],
  };
}

function inertProbePolicy(): FeaturePackagePolicy {
  return {
    ...fixturePolicy(),
    trustedKeys: [{
      keyId: "inert-probe-key",
      publicKey: M3A_INERT_PROBE_PUBLIC_KEY,
    }],
  };
}

async function withExecutionProbe(
  operation: () => Promise<void> | void,
): Promise<void> {
  const hadUserValue = Services.prefs.prefHasUserValue(SOURCE_PROBE_PREF);
  if (hadUserValue) {
    assertEquals(
      Services.prefs.getPrefType(SOURCE_PROBE_PREF),
      Services.prefs.PREF_BOOL,
      "pre-existing execution probe preference is boolean",
    );
  }
  const previousValue = hadUserValue
    ? Services.prefs.getBoolPref(SOURCE_PROBE_PREF)
    : false;
  try {
    Services.prefs.setBoolPref(SOURCE_PROBE_PREF, false);
    await operation();
  } finally {
    if (hadUserValue) {
      Services.prefs.setBoolPref(SOURCE_PROBE_PREF, previousValue);
    } else {
      Services.prefs.clearUserPref(SOURCE_PROBE_PREF);
    }
  }
}

function encodeBase64Url(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replaceAll("+", "-").replaceAll("/", "_").replace(
    /=+$/,
    "",
  );
}

function hex(bytes: Uint8Array): string {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join(
    "",
  );
}

function arrayBuffer(bytes: Uint8Array): ArrayBuffer {
  const buffer = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(buffer).set(bytes);
  return buffer;
}

async function dynamicBuildPackage(): Promise<{
  bytes: Uint8Array;
  policy: FeaturePackagePolicy;
}> {
  const pair = await crypto.subtle.generateKey({ name: "Ed25519" }, true, [
    "sign",
    "verify",
  ]) as CryptoKeyPair;
  const publicKey = encodeBase64Url(
    new Uint8Array(await crypto.subtle.exportKey("raw", pair.publicKey)),
  );
  const source = `Services.prefs.setBoolPref("${SOURCE_PROBE_PREF}", true);`;
  const sourceBytes = new TextEncoder().encode(source);
  const compatibility = {
    hostSchema: 2,
    hostBuildId: Services.appinfo.appBuildID,
    runtimeBuildIds: [Services.appinfo.platformBuildID],
    capabilities: [],
  };
  const manifest = {
    featureId: "hub-panel-menu",
    version: "native-test",
    releaseSequence: 2,
    sourceBytes: sourceBytes.length,
    sourceSha256: hex(
      new Uint8Array(
        await crypto.subtle.digest("SHA-256", arrayBuffer(sourceBytes)),
      ),
    ),
    compatibility,
  };
  const keyId = "native-test-key";
  const signingBytes = new TextEncoder().encode(
    "floorp-feature-package:v1\n" +
      JSON.stringify({ formatVersion: 1, keyId, manifest }),
  );
  const signature = encodeBase64Url(
    new Uint8Array(
      await crypto.subtle.sign(
        { name: "Ed25519" },
        pair.privateKey,
        arrayBuffer(signingBytes),
      ),
    ),
  );
  const envelope = JSON.stringify({
    formatVersion: 1,
    keyId,
    manifest,
    source,
    signature,
  });
  return {
    bytes: new TextEncoder().encode(envelope),
    policy: {
      featureId: "hub-panel-menu",
      hostSchema: 2,
      hostBuildId: Services.appinfo.appBuildID,
      runtimeBuildId: Services.appinfo.platformBuildID,
      allowedCapabilities: [],
      minimumReleaseSequence: 1,
      trustedKeys: [{ keyId, publicKey }],
    },
  };
}

async function verifyFixture(): Promise<VerifiedFeaturePackage> {
  return await verifier.verifySignedFeaturePackage(
    new TextEncoder().encode(M3A_FIXTURE_ENVELOPE),
    fixturePolicy(),
  );
}

const tests: TestCase[] = [
  {
    name:
      "packaged verifier accepts the public-only Deno cross-runtime fixture",
    fn: async () => {
      const result = await verifyFixture();
      assertEquals(
        result.manifest.featureId,
        "hub-panel-menu",
        "fixture feature ID matches",
      );
      assertEquals(
        result.manifest.version,
        "cross-runtime-v1",
        "fixture version matches",
      );
      assert(
        result.source.includes("__m3aFixtureSourceExecuted"),
        "source is returned as data",
      );
      assert(
        Object.isFrozen(result.manifest.compatibility.capabilities),
        "result arrays are frozen",
      );
    },
  },
  {
    name:
      "packaged verifier rejects altered signature, unknown key and incompatible policy",
    fn: async () => {
      const envelope = JSON.parse(M3A_FIXTURE_ENVELOPE) as Record<
        string,
        unknown
      >;
      const signature = String(envelope.signature);
      envelope.signature = (signature[0] === "A" ? "B" : "A") +
        signature.slice(1);
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(JSON.stringify(envelope)),
          fixturePolicy(),
        ),
        "invalid-signature",
      );
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(M3A_FIXTURE_ENVELOPE),
          {
            ...fixturePolicy(),
            trustedKeys: [],
          },
        ),
        "untrusted-key",
      );
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(M3A_FIXTURE_ENVELOPE),
          {
            ...fixturePolicy(),
            runtimeBuildId: "wrong-runtime",
          },
        ),
        "runtime-incompatible",
      );
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(M3A_FIXTURE_ENVELOPE),
          { ...fixturePolicy(), featureId: "other-feature" },
        ),
        "feature-mismatch",
      );
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(M3A_FIXTURE_ENVELOPE),
          { ...fixturePolicy(), hostBuildId: "wrong-host" },
        ),
        "host-incompatible",
      );
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(M3A_FIXTURE_ENVELOPE),
          { ...fixturePolicy(), allowedCapabilities: [] },
        ),
        "capability-denied",
      );
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(M3A_FIXTURE_ENVELOPE),
          { ...fixturePolicy(), minimumReleaseSequence: 2 },
        ),
        "sequence-too-low",
      );
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(M3A_FIXTURE_ENVELOPE),
          {
            ...fixturePolicy(),
            trustedKeys: [{ keyId: "fixture-key", publicKey: "A" }],
          },
        ),
        "invalid-policy",
      );
      const malformed = JSON.parse(M3A_FIXTURE_ENVELOPE) as Record<
        string,
        unknown
      >;
      (malformed.manifest as Record<string, unknown>).featureId = "Uppercase";
      await expectCode(
        verifier.verifySignedFeaturePackage(
          new TextEncoder().encode(JSON.stringify(malformed)),
          fixturePolicy(),
        ),
        "invalid-envelope",
      );
    },
  },
  {
    name:
      "test-pref probe detects cross-compartment source effects but verification stays inert",
    fn: async () => {
      await withExecutionProbe(() => {
        const sandbox = Components.utils.Sandbox(
          Services.scriptSecurityManager.getSystemPrincipal(),
          { sandboxName: "m3a-source-probe-test", freshCompartment: true },
        );
        try {
          (sandbox as unknown as Record<string, unknown>).Services = Services;
          Components.utils.evalInSandbox(
            `Services.prefs.setBoolPref("${SOURCE_PROBE_PREF}", true);`,
            sandbox,
          );
          assertEquals(
            Services.prefs.getBoolPref(SOURCE_PROBE_PREF),
            true,
            "probe observes a side effect from another compartment",
          );
        } finally {
          Components.utils.nukeSandbox(sandbox);
        }
      });

      await withExecutionProbe(async () => {
        const bytes = new TextEncoder().encode(M3A_INERT_PROBE_ENVELOPE);
        const policy = inertProbePolicy();
        const result = await verifier.verifySignedFeaturePackage(bytes, policy);
        assertEquals(result.keyId, "inert-probe-key", "probe fixture verifies");
        assertEquals(
          Services.prefs.getBoolPref(SOURCE_PROBE_PREF),
          false,
          "successful verification leaves the cross-compartment probe unset",
        );

        const tampered = JSON.parse(M3A_INERT_PROBE_ENVELOPE) as Record<
          string,
          unknown
        >;
        const signature = String(tampered.signature);
        tampered.signature = (signature[0] === "A" ? "B" : "A") +
          signature.slice(1);
        await expectCode(
          verifier.verifySignedFeaturePackage(
            new TextEncoder().encode(JSON.stringify(tampered)),
            policy,
          ),
          "invalid-signature",
        );
        assertEquals(
          Services.prefs.getBoolPref(SOURCE_PROBE_PREF),
          false,
          "rejected verification leaves the cross-compartment probe unset",
        );
      });
    },
  },
  {
    name: "execution probe rejects and preserves a non-boolean user preference",
    fn: async () => {
      const hadUserValue = Services.prefs.prefHasUserValue(SOURCE_PROBE_PREF);
      const previousType = Services.prefs.getPrefType(SOURCE_PROBE_PREF);
      const previousBoolean = hadUserValue &&
          previousType === Services.prefs.PREF_BOOL
        ? Services.prefs.getBoolPref(SOURCE_PROBE_PREF)
        : undefined;
      const previousString = hadUserValue &&
          previousType === Services.prefs.PREF_STRING
        ? Services.prefs.getStringPref(SOURCE_PROBE_PREF)
        : undefined;
      const previousInteger = hadUserValue &&
          previousType === Services.prefs.PREF_INT
        ? Services.prefs.getIntPref(SOURCE_PROBE_PREF)
        : undefined;
      try {
        Services.prefs.setStringPref(SOURCE_PROBE_PREF, "unrelated-value");
        let caught: unknown;
        try {
          await withExecutionProbe(() => {});
        } catch (error: unknown) {
          caught = error;
        }
        assert(
          caught instanceof Error,
          "non-boolean probe preference is refused",
        );
        assertEquals(
          Services.prefs.getPrefType(SOURCE_PROBE_PREF),
          Services.prefs.PREF_STRING,
          "non-boolean probe preference type remains unchanged",
        );
        assertEquals(
          Services.prefs.getStringPref(SOURCE_PROBE_PREF),
          "unrelated-value",
          "non-boolean probe preference value remains unchanged",
        );
      } finally {
        if (!hadUserValue) {
          Services.prefs.clearUserPref(SOURCE_PROBE_PREF);
        } else if (previousType === Services.prefs.PREF_BOOL) {
          Services.prefs.setBoolPref(
            SOURCE_PROBE_PREF,
            previousBoolean ?? false,
          );
        } else if (previousType === Services.prefs.PREF_STRING) {
          Services.prefs.setStringPref(SOURCE_PROBE_PREF, previousString ?? "");
        } else if (previousType === Services.prefs.PREF_INT) {
          Services.prefs.setIntPref(SOURCE_PROBE_PREF, previousInteger ?? 0);
        } else {
          Services.prefs.clearUserPref(SOURCE_PROBE_PREF);
        }
      }
    },
  },
  {
    name:
      "packaged verifier snapshots package bytes and policy before crypto awaits",
    fn: async () => {
      const bytes = new TextEncoder().encode(M3A_FIXTURE_ENVELOPE);
      const policy = fixturePolicy() as {
        featureId: string;
        hostSchema: number;
        hostBuildId: string;
        runtimeBuildId: string;
        allowedCapabilities: string[];
        minimumReleaseSequence: number;
        trustedKeys: { keyId: string; publicKey: string }[];
      };
      const pending = verifier.verifySignedFeaturePackage(bytes, policy);
      bytes.fill(0);
      policy.featureId = "changed";
      policy.trustedKeys[0].publicKey = "A";
      const result = await pending;
      assertEquals(
        result.manifest.featureId,
        "hub-panel-menu",
        "snapshot isolates caller mutation",
      );
    },
  },
  {
    name:
      "packaged verifier accepts runtime BuildID-bound Gecko WebCrypto signature",
    fn: async () => {
      await withExecutionProbe(async () => {
        const { bytes, policy } = await dynamicBuildPackage();
        const result = await verifier.verifySignedFeaturePackage(bytes, policy);
        assertEquals(
          result.manifest.compatibility.hostBuildId,
          Services.appinfo.appBuildID,
          "host BuildID is bound",
        );
        assert(
          result.manifest.compatibility.runtimeBuildIds.includes(
            Services.appinfo.platformBuildID,
          ),
          "runtime BuildID is admitted",
        );
        assertEquals(
          Services.prefs.getBoolPref(SOURCE_PROBE_PREF),
          false,
          "dynamic BuildID verification leaves the probe unset",
        );
      });
    },
  },
  {
    name:
      "packaged verifier accepts a Uint8Array from another Gecko compartment",
    fn: async () => {
      const pkg = await dynamicBuildPackage();
      const sandbox = Components.utils.Sandbox(
        Services.scriptSecurityManager.getSystemPrincipal(),
        { sandboxName: "m3a-cross-compartment-test", freshCompartment: true },
      );
      try {
        (sandbox as unknown as Record<string, unknown>).packageBytes =
          pkg.bytes;
        const foreignBytes = Components.utils.evalInSandbox(
          "new Uint8Array(packageBytes)",
          sandbox,
        ) as Uint8Array;
        const result = await verifier.verifySignedFeaturePackage(
          foreignBytes,
          pkg.policy,
        );
        assertEquals(result.keyId, "native-test-key", "foreign bytes verify");
      } finally {
        Components.utils.nukeSandbox(sandbox);
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("M3aFeaturePackageVerifierAcceptance.test.mts", tests);
}
