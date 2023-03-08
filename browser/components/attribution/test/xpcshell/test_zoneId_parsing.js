/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

/**
 * Test that we can properly parse and validate `zoneIdProvenanceData`.
 */

const { AttributionIOUtils } = ChromeUtils.importESModule(
  "resource:///modules/AttributionCode.sys.mjs"
);
const { ProvenanceData } = ChromeUtils.importESModule(
  "resource:///modules/ProvenanceData.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

add_setup(function setup() {
  let origReadUTF8 = AttributionIOUtils.readUTF8;
  registerCleanupFunction(() => {
    AttributionIOUtils.readUTF8 = origReadUTF8;
  });
});

// If `iniFileContents` is passed as `null`, we will simulate an error reading
// the INI.
async function testProvenance(iniFileContents, testFn, telemetryTestFn) {
  if (iniFileContents == null) {
    AttributionIOUtils.readUTF8 = async path => {
      throw new Error("test error: simulating provenance file read error");
    };
  } else {
    AttributionIOUtils.readUTF8 = async path => iniFileContents;
  }
  let provenance = await ProvenanceData.readZoneIdProvenanceFile();
  if (testFn) {
    await testFn(provenance);
  }
  if (telemetryTestFn) {
    Services.telemetry.clearScalars();
    await ProvenanceData.submitProvenanceTelemetry();
    let scalars = Services.telemetry.getSnapshotForScalars(
      "new-profile",
      false /* aClear */
    ).parent;
    let checkScalar = (scalarName, expectedValue) => {
      TelemetryTestUtils.assertScalar(scalars, scalarName, expectedValue);
    };
    telemetryTestFn(checkScalar);
  }
  ProvenanceData._clearCache();
}

add_task(async function unableToReadFile() {
  testProvenance(
    null,
    provenance => {
      Assert.ok("readProvenanceError" in provenance);
    },
    checkScalar => {
      checkScalar("attribution.provenance.data_exists", false);
    }
  );
});

add_task(async function expectedMozilla() {
  testProvenance(
    `
[Mozilla]
fileSystem=NTFS
zoneIdFileSize=194
zoneIdBufferLargeEnough=true
zoneIdTruncated=false

[MozillaZoneIdentifierStartSentinel]
[ZoneTransfer]
ZoneId=3
ReferrerUrl=https://mozilla.org/
HostUrl=https://download-installer.cdn.mozilla.net/pub/firefox/nightly/latest-mozilla-central-l10n/Firefox%20Installer.en-US.exe
`,
    provenance => {
      Assert.deepEqual(provenance, {
        fileSystem: "NTFS",
        zoneIdFileSize: 194,
        zoneIdBufferLargeEnough: true,
        zoneIdTruncated: false,
        zoneId: 3,
        referrerUrl: "https://mozilla.org/",
        hostUrl:
          "https://download-installer.cdn.mozilla.net/pub/firefox/nightly/latest-mozilla-central-l10n/Firefox%20Installer.en-US.exe",
        referrerUrlIsMozilla: true,
        hostUrlIsMozilla: true,
      });
    },
    checkScalar => {
      checkScalar("attribution.provenance.data_exists", true);
      checkScalar("attribution.provenance.file_system", "NTFS");
      checkScalar("attribution.provenance.ads_exists", true);
      checkScalar("attribution.provenance.security_zone", "3");
      checkScalar("attribution.provenance.referrer_url_exists", true);
      checkScalar("attribution.provenance.referrer_url_is_mozilla", true);
      checkScalar("attribution.provenance.host_url_exists", true);
      checkScalar("attribution.provenance.host_url_is_mozilla", true);
    }
  );
});

add_task(async function expectedNonMozilla() {
  testProvenance(
    `
[ZoneTransfer]
ReferrerUrl=https://mozilla.foobar.org/
HostUrl=https://download-installer.cdn.mozilla.foobar.net/pub/firefox/nightly/latest-mozilla-central-l10n/Firefox%20Installer.en-US.exe
`,
    provenance => {
      Assert.equal(provenance.referrerUrlIsMozilla, false);
      Assert.equal(provenance.hostUrlIsMozilla, false);
    },
    checkScalar => {
      checkScalar("attribution.provenance.referrer_url_exists", true);
      checkScalar("attribution.provenance.referrer_url_is_mozilla", false);
      checkScalar("attribution.provenance.host_url_exists", true);
      checkScalar("attribution.provenance.host_url_is_mozilla", false);
    }
  );
});

add_task(async function readFsError() {
  testProvenance(
    `
[Mozilla]
readFsError=openFile
readFsErrorCode=1234
`,
    provenance => {
      Assert.equal(provenance.readFsError, "openFile");
      Assert.equal(provenance.readFsErrorCode, 1234);
    },
    checkScalar => {
      checkScalar("attribution.provenance.file_system", "error");
    }
  );
});

add_task(async function unexpectedReadFsError() {
  testProvenance(
    `
[Mozilla]
readFsError=openFile
readFsErrorCode=bazqux
`,
    provenance => {
      Assert.equal(provenance.readFsError, "openFile");
      Assert.ok(!("readFsErrorCode" in provenance));
    },
    checkScalar => {
      checkScalar("attribution.provenance.file_system", "error");
    }
  );
});

add_task(async function unexpectedReadFsError() {
  testProvenance(
    `
[Mozilla]
readFsError=foobar
`,
    provenance => {
      Assert.equal(provenance.readFsError, "unexpected");
    },
    checkScalar => {
      checkScalar("attribution.provenance.file_system", "error");
    }
  );
});

add_task(async function missingFileSystem() {
  testProvenance(
    ``,
    provenance => {
      Assert.equal(provenance.fileSystem, "missing");
    },
    checkScalar => {
      checkScalar("attribution.provenance.file_system", "missing");
    }
  );
});

add_task(async function fileSystem() {
  testProvenance(
    `
[Mozilla]
fileSystem=ntfs
`,
    provenance => {
      Assert.equal(provenance.fileSystem, "NTFS");
    },
    checkScalar => {
      checkScalar("attribution.provenance.file_system", "NTFS");
    }
  );
});

add_task(async function unexpectedFileSystem() {
  testProvenance(
    `
[Mozilla]
fileSystem=foobar
`,
    provenance => {
      Assert.equal(provenance.fileSystem, "other");
    },
    checkScalar => {
      checkScalar("attribution.provenance.file_system", "other");
    }
  );
});

add_task(async function zoneIdFileSize() {
  testProvenance(
    `
[Mozilla]
zoneIdFileSize=1234
`,
    provenance => {
      Assert.equal(provenance.zoneIdFileSize, 1234);
    },
    null
  );
});

add_task(async function unknownZoneIdFileSize() {
  testProvenance(
    `
[Mozilla]
zoneIdFileSize=unknown
`,
    provenance => {
      Assert.equal(provenance.zoneIdFileSize, "unknown");
    },
    null
  );
});

add_task(async function unexpectedZoneIdFileSize() {
  testProvenance(
    `
[Mozilla]
zoneIdFileSize=foobar
`,
    provenance => {
      Assert.equal(provenance.zoneIdFileSize, "unexpected");
    },
    null
  );
});

add_task(async function missingZoneIdFileSize() {
  testProvenance(
    ``,
    provenance => {
      Assert.ok(!("zoneIdFileSize" in provenance));
    },
    null
  );
});

add_task(async function zoneIdBufferLargeEnoughTrue() {
  testProvenance(
    `
[Mozilla]
zoneIdBufferLargeEnough=TrUe
`,
    provenance => {
      Assert.equal(provenance.zoneIdBufferLargeEnough, true);
    },
    null
  );
});

add_task(async function zoneIdBufferLargeEnoughFalse() {
  testProvenance(
    `
[Mozilla]
zoneIdBufferLargeEnough=FaLsE
`,
    provenance => {
      Assert.equal(provenance.zoneIdBufferLargeEnough, false);
    },
    null
  );
});

add_task(async function unknownZoneIdBufferLargeEnough() {
  testProvenance(
    `
[Mozilla]
zoneIdBufferLargeEnough=unknown
`,
    provenance => {
      Assert.equal(provenance.zoneIdBufferLargeEnough, "unknown");
    },
    null
  );
});

add_task(async function unknownZoneIdBufferLargeEnough() {
  testProvenance(
    `
[Mozilla]
zoneIdBufferLargeEnough=foobar
`,
    provenance => {
      Assert.equal(provenance.zoneIdBufferLargeEnough, "unexpected");
    },
    null
  );
});

add_task(async function missingZoneIdBufferLargeEnough() {
  testProvenance(``, provenance => {
    Assert.ok(!("zoneIdBufferLargeEnough" in provenance));
  });
});

add_task(async function zoneIdTruncatedTrue() {
  testProvenance(
    `
[Mozilla]
zoneIdTruncated=TrUe
`,
    provenance => {
      Assert.equal(provenance.zoneIdTruncated, true);
    },
    null
  );
});

add_task(async function zoneIdTruncatedFalse() {
  testProvenance(
    `
[Mozilla]
zoneIdTruncated=FaLsE
`,
    provenance => {
      Assert.equal(provenance.zoneIdTruncated, false);
    },
    null
  );
});

add_task(async function unknownZoneIdTruncated() {
  testProvenance(
    `
[Mozilla]
zoneIdTruncated=unknown
`,
    provenance => {
      Assert.equal(provenance.zoneIdTruncated, "unknown");
    },
    null
  );
});

add_task(async function unexpectedZoneIdTruncated() {
  testProvenance(
    `
[Mozilla]
zoneIdTruncated=foobar
`,
    provenance => {
      Assert.equal(provenance.zoneIdTruncated, "unexpected");
    },
    null
  );
});

add_task(async function missingZoneIdTruncated() {
  testProvenance(``, provenance => {
    Assert.ok(!("zoneIdTruncated" in provenance));
  });
});

add_task(async function readZoneIdError() {
  testProvenance(
    `
[Mozilla]
readZoneIdError=openFile
readZoneIdErrorCode=1234
`,
    provenance => {
      Assert.equal(provenance.readZoneIdError, "openFile");
      Assert.equal(provenance.readZoneIdErrorCode, 1234);
    },
    null
  );
});

add_task(async function unexpectedReadZoneIdErrorCode() {
  testProvenance(
    `
[Mozilla]
readZoneIdError=openFile
readZoneIdErrorCode=bazqux
`,
    provenance => {
      Assert.equal(provenance.readZoneIdError, "openFile");
      Assert.ok(!("readZoneIdErrorCode" in provenance));
    },
    null
  );
});

add_task(async function noAdsOnInstaller() {
  testProvenance(
    `
[Mozilla]
readZoneIdError=openFile
readZoneIdErrorCode=2
`,
    provenance => {
      Assert.equal(provenance.readZoneIdError, "openFile");
      Assert.equal(provenance.readZoneIdErrorCode, 2);
    },
    checkScalar => {
      checkScalar("attribution.provenance.ads_exists", false);
    }
  );
});

add_task(async function unexpectedReadZoneIdError() {
  testProvenance(
    `
[Mozilla]
readZoneIdError=foobar
`,
    provenance => {
      Assert.equal(provenance.readZoneIdError, "unexpected");
    },
    null
  );
});

add_task(async function missingZoneId() {
  testProvenance(
    ``,
    provenance => {
      Assert.equal(provenance.zoneId, "missing");
    },
    null
  );
});

add_task(async function unexpectedZoneId() {
  testProvenance(
    `
[ZoneTransfer]
ZoneId=9999999999
`,
    provenance => {
      Assert.equal(provenance.zoneId, "unexpected");
    },
    checkScalar => {
      checkScalar("attribution.provenance.ads_exists", true);
      checkScalar("attribution.provenance.security_zone", "unexpected");
    }
  );
});

add_task(async function missingReferrerUrl() {
  testProvenance(
    ``,
    provenance => {
      Assert.equal(provenance.referrerUrl, "missing");
    },
    checkScalar => {
      checkScalar("attribution.provenance.ads_exists", true);
      checkScalar("attribution.provenance.referrer_url_exists", false);
    }
  );
});

add_task(async function unexpectedReferrerUrl() {
  testProvenance(
    `
[ZoneTransfer]
ReferrerUrl=foobar
`,
    provenance => {
      Assert.equal(provenance.referrerUrl, "unexpected");
    },
    null
  );
});

add_task(async function missingHostUrl() {
  testProvenance(
    ``,
    provenance => {
      Assert.equal(provenance.hostUrl, "missing");
    },
    checkScalar => {
      checkScalar("attribution.provenance.ads_exists", true);
      checkScalar("attribution.provenance.referrer_url_exists", false);
    }
  );
});

add_task(async function unexpectedHostUrl() {
  testProvenance(
    `
[ZoneTransfer]
HostUrl=foobar
`,
    provenance => {
      Assert.equal(provenance.hostUrl, "unexpected");
    },
    null
  );
});
