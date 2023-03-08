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

add_setup(function setup() {
  let origReadUTF8 = AttributionIOUtils.readUTF8;
  registerCleanupFunction(() => {
    AttributionIOUtils.readUTF8 = origReadUTF8;
  });
});

// If `iniFileContents` is passed as `null`, we will simulate an error reading
// the INI.
async function testProvenance(iniFileContents, testFn) {
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
  ProvenanceData._clearCache();
}

add_task(async function unableToReadFile() {
  testProvenance(null, provenance => {
    Assert.ok("readProvenanceError" in provenance);
  });
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
    }
  );
});

add_task(async function missingFileSystem() {
  testProvenance(``, provenance => {
    Assert.equal(provenance.fileSystem, "missing");
  });
});

add_task(async function fileSystem() {
  testProvenance(
    `
[Mozilla]
fileSystem=ntfs
`,
    provenance => {
      Assert.equal(provenance.fileSystem, "NTFS");
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
    }
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
    }
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
    }
  );
});

add_task(async function missingZoneIdFileSize() {
  testProvenance(``, provenance => {
    Assert.ok(!("zoneIdFileSize" in provenance));
  });
});

add_task(async function zoneIdBufferLargeEnoughTrue() {
  testProvenance(
    `
[Mozilla]
zoneIdBufferLargeEnough=TrUe
`,
    provenance => {
      Assert.equal(provenance.zoneIdBufferLargeEnough, true);
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
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
    }
  );
});

add_task(async function missingZoneId() {
  testProvenance(``, provenance => {
    Assert.equal(provenance.zoneId, "missing");
  });
});

add_task(async function unexpectedZoneId() {
  testProvenance(
    `
[ZoneTransfer]
ZoneId=9999999999
`,
    provenance => {
      Assert.equal(provenance.zoneId, "unexpected");
    }
  );
});

add_task(async function missingReferrerUrl() {
  testProvenance(``, provenance => {
    Assert.equal(provenance.referrerUrl, "missing");
  });
});

add_task(async function unexpectedReferrerUrl() {
  testProvenance(
    `
[ZoneTransfer]
ReferrerUrl=foobar
`,
    provenance => {
      Assert.equal(provenance.referrerUrl, "unexpected");
    }
  );
});

add_task(async function missingHostUrl() {
  testProvenance(``, provenance => {
    Assert.equal(provenance.hostUrl, "missing");
  });
});

add_task(async function unexpectedHostUrl() {
  testProvenance(
    `
[ZoneTransfer]
HostUrl=foobar
`,
    provenance => {
      Assert.equal(provenance.hostUrl, "unexpected");
    }
  );
});
