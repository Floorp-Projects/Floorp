"use strict";

const TEST_STORE_FILE_NAME = "test-profile.json";
const { CREDIT_CARD_SCHEMA_VERSION } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillStorageBase.sys.mjs"
);

// NOTE: a guide to reading these test-cases:
// parent: What the local record looked like the last time we wrote the
//         record to the Sync server.
// local:  What the local record looks like now. IOW, the differences between
//         'parent' and 'local' are changes recently made which we wish to sync.
// remote: An incoming record we need to apply (ie, a record that was possibly
//         changed on a remote device)
//
// To further help understanding this, a few of the testcases are annotated.
const ADDRESS_RECONCILE_TESTCASES = [
  {
    description: "Local change",
    parent: {
      // So when we last wrote the record to the server, it had these values.
      guid: "2bbd2d8fbc6b",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    local: [
      {
        // The current local record - by comparing against parent we can see that
        // only the name has changed locally.
        name: "Skip",
        "street-address": "32 Vassar Street",
      },
    ],
    remote: {
      // This is the incoming record. It has the same values as "parent", so
      // we can deduce the record hasn't actually been changed remotely so we
      // can safely ignore the incoming record and write our local changes.
      guid: "2bbd2d8fbc6b",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    reconciled: {
      guid: "2bbd2d8fbc6b",
      name: "Skip",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description: "Remote change",
    parent: {
      guid: "e3680e9f890d",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    local: [
      {
        name: "Mark Hammond",
        "street-address": "32 Vassar Street",
      },
    ],
    remote: {
      guid: "e3680e9f890d",
      version: 1,
      name: "Skip",
      "street-address": "32 Vassar Street",
    },
    reconciled: {
      guid: "e3680e9f890d",
      name: "Skip",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description: "New local field",
    parent: {
      guid: "0cba738b1be0",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    local: [
      {
        name: "Mark Hammond",
        "street-address": "32 Vassar Street",
        tel: "123456",
      },
    ],
    remote: {
      guid: "0cba738b1be0",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    reconciled: {
      guid: "0cba738b1be0",
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      tel: "123456",
    },
  },
  {
    description: "New remote field",
    parent: {
      guid: "be3ef97f8285",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    local: [
      {
        name: "Mark Hammond",
        "street-address": "32 Vassar Street",
      },
    ],
    remote: {
      guid: "be3ef97f8285",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      tel: "123456",
    },
    reconciled: {
      guid: "be3ef97f8285",
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      tel: "123456",
    },
  },
  {
    description: "Deleted field locally",
    parent: {
      guid: "9627322248ec",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      tel: "123456",
    },
    local: [
      {
        name: "Mark Hammond",
        "street-address": "32 Vassar Street",
      },
    ],
    remote: {
      guid: "9627322248ec",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      tel: "123456",
    },
    reconciled: {
      guid: "9627322248ec",
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description: "Deleted field remotely",
    parent: {
      guid: "7d7509f3eeb2",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      tel: "123456",
    },
    local: [
      {
        name: "Mark Hammond",
        "street-address": "32 Vassar Street",
        tel: "123456",
      },
    ],
    remote: {
      guid: "7d7509f3eeb2",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    reconciled: {
      guid: "7d7509f3eeb2",
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description: "Local and remote changes to unrelated fields",
    parent: {
      // The last time we wrote this to the server, country was NZ.
      guid: "e087a06dfc57",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      country: "NZ",
      // We also had an unknown field we round-tripped
      foo: "bar",
    },
    local: [
      {
        // The current local record - so locally we've changed name to Skip.
        name: "Skip",
        "street-address": "32 Vassar Street",
        country: "NZ",
      },
    ],
    remote: {
      // Remotely, we've changed the country to AU.
      guid: "e087a06dfc57",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      country: "AU",
      // This is a new unknown field that should send instead!
      "unknown-1": "an unknown field from another client",
    },
    reconciled: {
      guid: "e087a06dfc57",
      name: "Skip",
      "street-address": "32 Vassar Street",
      country: "AU",
      // This is a new unknown field that should send instead!
      "unknown-1": "an unknown field from another client",
    },
  },
  {
    description: "Multiple local changes",
    parent: {
      guid: "340a078c596f",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      tel: "123456",
      country: "US",
    },
    local: [
      {
        name: "Skip",
        "street-address": "32 Vassar Street",
        country: "US",
      },
      {
        name: "Skip",
        "street-address": "32 Vassar Street",
        organization: "Mozilla",
        country: "US",
      },
    ],
    remote: {
      guid: "340a078c596f",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      tel: "123456",
      country: "AU",
    },
    reconciled: {
      guid: "340a078c596f",
      name: "Skip",
      "street-address": "32 Vassar Street",
      organization: "Mozilla",
      country: "AU",
    },
  },
  {
    // Local and remote diverged from the shared parent, but the values are the
    // same, so we shouldn't fork.
    description: "Same change to local and remote",
    parent: {
      guid: "0b3a72a1bea2",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      // unknown fields we previously roundtripped
      foo: "bar",
    },
    local: [
      {
        name: "Skip",
        "street-address": "32 Vassar Street",
      },
    ],
    remote: {
      guid: "0b3a72a1bea2",
      version: 1,
      name: "Skip",
      "street-address": "32 Vassar Street",
      // New unknown field that should be the new round trip
      "unknown-1": "an unknown field from another client",
    },
    reconciled: {
      guid: "0b3a72a1bea2",
      name: "Skip",
      "street-address": "32 Vassar Street",
    },
  },
  {
    description: "Conflicting changes to single field",
    parent: {
      // This is what we last wrote to the sync server.
      guid: "62068784d089",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      "unknown-1": "an unknown field from another client",
    },
    local: [
      {
        // The current version of the local record - the name has changed locally.
        name: "Skip",
        "street-address": "32 Vassar Street",
      },
    ],
    remote: {
      // An incoming record has a different name than any of the above!
      guid: "62068784d089",
      version: 1,
      name: "Kip",
      "street-address": "32 Vassar Street",
      "unknown-1": "an unknown field from another client",
    },
    forked: {
      // So we've forked the local record to a new GUID (and the next sync is
      // going to write this as a new record)
      name: "Skip",
      "street-address": "32 Vassar Street",
      "unknown-1": "an unknown field from another client",
    },
    reconciled: {
      // And we've updated the local version of the record to be the remote version.
      guid: "62068784d089",
      name: "Kip",
      "street-address": "32 Vassar Street",
      "unknown-1": "an unknown field from another client",
    },
  },
  {
    description: "Conflicting changes to multiple fields",
    parent: {
      guid: "244dbb692e94",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      country: "NZ",
    },
    local: [
      {
        name: "Skip",
        "street-address": "32 Vassar Street",
        country: "AU",
      },
    ],
    remote: {
      guid: "244dbb692e94",
      version: 1,
      name: "Kip",
      "street-address": "32 Vassar Street",
      country: "CA",
    },
    forked: {
      name: "Skip",
      "street-address": "32 Vassar Street",
      country: "AU",
    },
    reconciled: {
      guid: "244dbb692e94",
      name: "Kip",
      "street-address": "32 Vassar Street",
      country: "CA",
    },
  },
  {
    description: "Field deleted locally, changed remotely",
    parent: {
      guid: "6fc45e03d19a",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      country: "AU",
    },
    local: [
      {
        name: "Mark Hammond",
        "street-address": "32 Vassar Street",
      },
    ],
    remote: {
      guid: "6fc45e03d19a",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      country: "NZ",
    },
    forked: {
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    reconciled: {
      guid: "6fc45e03d19a",
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      country: "NZ",
    },
  },
  {
    description: "Field changed locally, deleted remotely",
    parent: {
      guid: "fff9fa27fa18",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      country: "AU",
    },
    local: [
      {
        name: "Mark Hammond",
        "street-address": "32 Vassar Street",
        country: "NZ",
      },
    ],
    remote: {
      guid: "fff9fa27fa18",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
    forked: {
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      country: "NZ",
    },
    reconciled: {
      guid: "fff9fa27fa18",
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    },
  },
  {
    // Created, last modified should be synced; last used and times used should
    // be local. Remote created time older than local, remote modified time
    // newer than local.
    description:
      "Created, last modified time reconciliation without local changes",
    parent: {
      guid: "5113f329c42f",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      timeCreated: 1234,
      timeLastModified: 5678,
      timeLastUsed: 5678,
      timesUsed: 6,
    },
    local: [],
    remote: {
      guid: "5113f329c42f",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      timeCreated: 1200,
      timeLastModified: 5700,
      timeLastUsed: 5700,
      timesUsed: 3,
    },
    reconciled: {
      guid: "5113f329c42f",
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      timeCreated: 1200,
      timeLastModified: 5700,
      timeLastUsed: 5678,
      timesUsed: 6,
    },
  },
  {
    // Local changes, remote created time newer than local, remote modified time
    // older than local.
    description:
      "Created, last modified time reconciliation with local changes",
    parent: {
      guid: "791e5608b80a",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      timeCreated: 1234,
      timeLastModified: 5678,
      timeLastUsed: 5678,
      timesUsed: 6,
    },
    local: [
      {
        name: "Skip",
        "street-address": "32 Vassar Street",
      },
    ],
    remote: {
      guid: "791e5608b80a",
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      timeCreated: 1300,
      timeLastModified: 5000,
      timeLastUsed: 5000,
      timesUsed: 3,
    },
    reconciled: {
      guid: "791e5608b80a",
      name: "Skip",
      "street-address": "32 Vassar Street",
      timeCreated: 1234,
      timeLastUsed: 5678,
      timesUsed: 6,
    },
  },
];

const CREDIT_CARD_RECONCILE_TESTCASES = [
  {
    description: "Local change",
    parent: {
      // So when we last wrote the record to the server, it had these values.
      guid: "2bbd2d8fbc6b",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "unknown-1": "an unknown field from another client",
    },
    local: [
      {
        // The current local record - by comparing against parent we can see that
        // only the cc-number has changed locally.
        "cc-name": "John Doe",
        "cc-number": "4929001587121045",
      },
    ],
    remote: {
      // This is the incoming record. It has the same values as "parent", so
      // we can deduce the record hasn't actually been changed remotely so we
      // can safely ignore the incoming record and write our local changes.
      guid: "2bbd2d8fbc6b",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "unknown-2": "a newer unknown field",
    },
    reconciled: {
      guid: "2bbd2d8fbc6b",
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "unknown-2": "a newer unknown field",
    },
  },
  {
    description: "Remote change",
    parent: {
      guid: "e3680e9f890d",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "unknown-1": "unknown field",
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4111111111111111",
      },
    ],
    remote: {
      guid: "e3680e9f890d",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "unknown-1": "unknown field",
    },
    reconciled: {
      guid: "e3680e9f890d",
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "unknown-1": "unknown field",
    },
  },

  {
    description: "New local field",
    parent: {
      guid: "0cba738b1be0",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4111111111111111",
        "cc-exp-month": 12,
      },
    ],
    remote: {
      guid: "0cba738b1be0",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
    reconciled: {
      guid: "0cba738b1be0",
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
  },
  {
    description: "New remote field",
    parent: {
      guid: "be3ef97f8285",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4111111111111111",
      },
    ],
    remote: {
      guid: "be3ef97f8285",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
    reconciled: {
      guid: "be3ef97f8285",
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
  },
  {
    description: "Deleted field locally",
    parent: {
      guid: "9627322248ec",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4111111111111111",
      },
    ],
    remote: {
      guid: "9627322248ec",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
    reconciled: {
      guid: "9627322248ec",
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
  },
  {
    description: "Deleted field remotely",
    parent: {
      guid: "7d7509f3eeb2",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4111111111111111",
        "cc-exp-month": 12,
      },
    ],
    remote: {
      guid: "7d7509f3eeb2",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
    reconciled: {
      guid: "7d7509f3eeb2",
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
  },
  {
    description: "Local and remote changes to unrelated fields",
    parent: {
      // The last time we wrote this to the server, "cc-exp-month" was 12.
      guid: "e087a06dfc57",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
      "unknown-1": "unknown field",
    },
    local: [
      {
        // The current local record - so locally we've changed "cc-number".
        "cc-name": "John Doe",
        "cc-number": "4929001587121045",
        "cc-exp-month": 12,
      },
    ],
    remote: {
      // Remotely, we've changed "cc-exp-month" to 1.
      guid: "e087a06dfc57",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 1,
      "unknown-2": "a newer unknown field",
    },
    reconciled: {
      guid: "e087a06dfc57",
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "cc-exp-month": 1,
      "unknown-2": "a newer unknown field",
    },
  },
  {
    description: "Multiple local changes",
    parent: {
      guid: "340a078c596f",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "unknown-1": "unknown field",
    },
    local: [
      {
        "cc-name": "Skip",
        "cc-number": "4111111111111111",
      },
      {
        "cc-name": "Skip",
        "cc-number": "4111111111111111",
        "cc-exp-month": 12,
      },
    ],
    remote: {
      guid: "340a078c596f",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-year": 2000,
      "unknown-1": "unknown field",
    },
    reconciled: {
      guid: "340a078c596f",
      "cc-name": "Skip",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
      "unknown-1": "unknown field",
    },
  },
  {
    // Local and remote diverged from the shared parent, but the values are the
    // same, so we shouldn't fork.
    description: "Same change to local and remote",
    parent: {
      guid: "0b3a72a1bea2",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4929001587121045",
      },
    ],
    remote: {
      guid: "0b3a72a1bea2",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
    },
    reconciled: {
      guid: "0b3a72a1bea2",
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
    },
  },
  {
    description: "Conflicting changes to single field",
    parent: {
      // This is what we last wrote to the sync server.
      guid: "62068784d089",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "unknown-1": "unknown field",
    },
    local: [
      {
        // The current version of the local record - the cc-number has changed locally.
        "cc-name": "John Doe",
        "cc-number": "5103059495477870",
      },
    ],
    remote: {
      // An incoming record has a different cc-number than any of the above!
      guid: "62068784d089",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "unknown-1": "unknown field",
    },
    forked: {
      // So we've forked the local record to a new GUID (and the next sync is
      // going to write this as a new record)
      "cc-name": "John Doe",
      "cc-number": "5103059495477870",
      "unknown-1": "unknown field",
    },
    reconciled: {
      // And we've updated the local version of the record to be the remote version.
      guid: "62068784d089",
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "unknown-1": "unknown field",
    },
  },
  {
    description: "Conflicting changes to multiple fields",
    parent: {
      guid: "244dbb692e94",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "5103059495477870",
        "cc-exp-month": 1,
      },
    ],
    remote: {
      guid: "244dbb692e94",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "cc-exp-month": 3,
    },
    forked: {
      "cc-name": "John Doe",
      "cc-number": "5103059495477870",
      "cc-exp-month": 1,
    },
    reconciled: {
      guid: "244dbb692e94",
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      "cc-exp-month": 3,
    },
  },
  {
    description: "Field deleted locally, changed remotely",
    parent: {
      guid: "6fc45e03d19a",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4111111111111111",
      },
    ],
    remote: {
      guid: "6fc45e03d19a",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 3,
    },
    forked: {
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
    reconciled: {
      guid: "6fc45e03d19a",
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 3,
    },
  },
  {
    description: "Field changed locally, deleted remotely",
    parent: {
      guid: "fff9fa27fa18",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 12,
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4111111111111111",
        "cc-exp-month": 3,
      },
    ],
    remote: {
      guid: "fff9fa27fa18",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
    forked: {
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      "cc-exp-month": 3,
    },
    reconciled: {
      guid: "fff9fa27fa18",
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
    },
  },
  {
    // Created, last modified should be synced; last used and times used should
    // be local. Remote created time older than local, remote modified time
    // newer than local.
    description:
      "Created, last modified time reconciliation without local changes",
    parent: {
      guid: "5113f329c42f",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      timeCreated: 1234,
      timeLastModified: 5678,
      timeLastUsed: 5678,
      timesUsed: 6,
    },
    local: [],
    remote: {
      guid: "5113f329c42f",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      timeCreated: 1200,
      timeLastModified: 5700,
      timeLastUsed: 5700,
      timesUsed: 3,
    },
    reconciled: {
      guid: "5113f329c42f",
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      timeCreated: 1200,
      timeLastModified: 5700,
      timeLastUsed: 5678,
      timesUsed: 6,
    },
  },
  {
    // Local changes, remote created time newer than local, remote modified time
    // older than local.
    description:
      "Created, last modified time reconciliation with local changes",
    parent: {
      guid: "791e5608b80a",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      timeCreated: 1234,
      timeLastModified: 5678,
      timeLastUsed: 5678,
      timesUsed: 6,
    },
    local: [
      {
        "cc-name": "John Doe",
        "cc-number": "4929001587121045",
      },
    ],
    remote: {
      guid: "791e5608b80a",
      version: CREDIT_CARD_SCHEMA_VERSION,
      "cc-name": "John Doe",
      "cc-number": "4111111111111111",
      timeCreated: 1300,
      timeLastModified: 5000,
      timeLastUsed: 5000,
      timesUsed: 3,
    },
    reconciled: {
      guid: "791e5608b80a",
      "cc-name": "John Doe",
      "cc-number": "4929001587121045",
      timeCreated: 1234,
      timeLastUsed: 5678,
      timesUsed: 6,
    },
  },
];

add_task(async function test_reconcile_unknown_version() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);

  // Cross-version reconciliation isn't supported yet. See bug 1377204.
  await Assert.rejects(
    profileStorage.addresses.reconcile({
      guid: "31d83d2725ec",
      version: 3,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
    }),
    /Got unknown record version/
  );
});

add_task(async function test_reconcile_idempotent() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);

  let guid = "de1ba7b094fe";
  await profileStorage.addresses.add(
    {
      guid,
      version: 1,
      name: "Mark Hammond",
      "street-address": "32 Vassar Street",
      // an unknown field from a previous sync
      foo: "bar",
    },
    { sourceSync: true }
  );
  await profileStorage.addresses.update(guid, {
    name: "Skip",
    "street-address": "32 Vassar Street",
    organization: "Mozilla",
  });

  let remote = {
    guid,
    version: 1,
    name: "Mark Hammond",
    "street-address": "32 Vassar Street",
    tel: "123456",
    "unknown-1": "an unknown field from another client",
  };

  {
    let { forkedGUID } = await profileStorage.addresses.reconcile(remote);
    let updatedRecord = await profileStorage.addresses.get(guid, {
      rawData: true,
    });

    ok(!forkedGUID, "First merge should not fork record");
    ok(
      objectMatches(updatedRecord, {
        guid: "de1ba7b094fe",
        name: "Skip",
        "street-address": "32 Vassar Street",
        organization: "Mozilla",
        tel: "123456",
        "unknown-1": "an unknown field from another client",
      }),
      "First merge should merge local and remote changes"
    );
  }

  {
    let { forkedGUID } = await profileStorage.addresses.reconcile(remote);
    let updatedRecord = await profileStorage.addresses.get(guid, {
      rawData: true,
    });

    ok(!forkedGUID, "Second merge should not fork record");
    ok(
      objectMatches(updatedRecord, {
        guid: "de1ba7b094fe",
        name: "Skip",
        "street-address": "32 Vassar Street",
        organization: "Mozilla",
        tel: "123456",
        "unknown-1": "an unknown field from another client",
      }),
      "Second merge should not change record"
    );
  }
});

add_task(async function test_reconcile_three_way_merge() {
  let TESTCASES = {
    addresses: ADDRESS_RECONCILE_TESTCASES,
    creditCards: CREDIT_CARD_RECONCILE_TESTCASES,
  };

  for (let collectionName in TESTCASES) {
    info(`Start to test reconcile on ${collectionName}`);

    let profileStorage = await initProfileStorage(
      TEST_STORE_FILE_NAME,
      null,
      collectionName
    );

    for (let test of TESTCASES[collectionName]) {
      info(test.description);

      await profileStorage[collectionName].add(test.parent, {
        sourceSync: true,
      });

      for (let updatedRecord of test.local) {
        await profileStorage[collectionName].update(
          test.parent.guid,
          updatedRecord
        );
      }

      let localRecord = await profileStorage[collectionName].get(
        test.parent.guid,
        {
          rawData: true,
        }
      );

      let onReconciled = TestUtils.topicObserved(
        "formautofill-storage-changed",
        (subject, data) =>
          data == "reconcile" &&
          subject.wrappedJSObject.collectionName == collectionName
      );
      let { forkedGUID } = await profileStorage[collectionName].reconcile(
        test.remote
      );
      await onReconciled;
      let reconciledRecord = await profileStorage[collectionName].get(
        test.parent.guid,
        {
          rawData: true,
        }
      );
      if (forkedGUID) {
        let forkedRecord = await profileStorage[collectionName].get(
          forkedGUID,
          {
            rawData: true,
          }
        );

        notEqual(forkedRecord.guid, reconciledRecord.guid);
        equal(forkedRecord.timeLastModified, localRecord.timeLastModified);
        ok(
          objectMatches(forkedRecord, test.forked),
          `${test.description} should fork record`
        );
      } else {
        ok(!test.forked, `${test.description} should not fork record`);
      }

      ok(objectMatches(reconciledRecord, test.reconciled));
    }
  }
});
