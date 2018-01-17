"use strict";

const TEST_STORE_FILE_NAME = "test-profile.json";

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
      "guid": "2bbd2d8fbc6b",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    local: [{
      // The current local record - by comparing against parent we can see that
      // only the given-name has changed locally.
      "given-name": "Skip",
      "family-name": "Hammond",
    }],
    remote: {
      // This is the incoming record. It has the same values as "parent", so
      // we can deduce the record hasn't actually been changed remotely so we
      // can safely ignore the incoming record and write our local changes.
      "guid": "2bbd2d8fbc6b",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    reconciled: {
      "guid": "2bbd2d8fbc6b",
      "given-name": "Skip",
      "family-name": "Hammond",
    },
  },
  {
    description: "Remote change",
    parent: {
      "guid": "e3680e9f890d",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    local: [{
      "given-name": "Mark",
      "family-name": "Hammond",
    }],
    remote: {
      "guid": "e3680e9f890d",
      "version": 1,
      "given-name": "Skip",
      "family-name": "Hammond",
    },
    reconciled: {
      "guid": "e3680e9f890d",
      "given-name": "Skip",
      "family-name": "Hammond",
    },
  },
  {
    description: "New local field",
    parent: {
      "guid": "0cba738b1be0",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    local: [{
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    }],
    remote: {
      "guid": "0cba738b1be0",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    reconciled: {
      "guid": "0cba738b1be0",
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    },
  },
  {
    description: "New remote field",
    parent: {
      "guid": "be3ef97f8285",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    local: [{
      "given-name": "Mark",
      "family-name": "Hammond",
    }],
    remote: {
      "guid": "be3ef97f8285",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    },
    reconciled: {
      "guid": "be3ef97f8285",
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    },
  },
  {
    description: "Deleted field locally",
    parent: {
      "guid": "9627322248ec",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    },
    local: [{
      "given-name": "Mark",
      "family-name": "Hammond",
    }],
    remote: {
      "guid": "9627322248ec",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    },
    reconciled: {
      "guid": "9627322248ec",
      "given-name": "Mark",
      "family-name": "Hammond",
    },
  },
  {
    description: "Deleted field remotely",
    parent: {
      "guid": "7d7509f3eeb2",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    },
    local: [{
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    }],
    remote: {
      "guid": "7d7509f3eeb2",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    reconciled: {
      "guid": "7d7509f3eeb2",
      "given-name": "Mark",
      "family-name": "Hammond",
    },
  },
  {
    description: "Local and remote changes to unrelated fields",
    parent: {
      // The last time we wrote this to the server, country was NZ.
      "guid": "e087a06dfc57",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "NZ",
    },
    local: [{
      // The current local record - so locally we've changed given-name to Skip.
      "given-name": "Skip",
      "family-name": "Hammond",
      "country": "NZ",
    }],
    remote: {
      // Remotely, we've changed the country to AU.
      "guid": "e087a06dfc57",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "AU",
    },
    reconciled: {
      "guid": "e087a06dfc57",
      "given-name": "Skip",
      "family-name": "Hammond",
      "country": "AU",
    },
  },
  {
    description: "Multiple local changes",
    parent: {
      "guid": "340a078c596f",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
    },
    local: [{
      "given-name": "Skip",
      "family-name": "Hammond",
    }, {
      "given-name": "Skip",
      "family-name": "Hammond",
      "organization": "Mozilla",
    }],
    remote: {
      "guid": "340a078c596f",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "tel": "123456",
      "country": "AU",
    },
    reconciled: {
      "guid": "340a078c596f",
      "given-name": "Skip",
      "family-name": "Hammond",
      "organization": "Mozilla",
      "country": "AU",
    },
  },
  {
    // Local and remote diverged from the shared parent, but the values are the
    // same, so we shouldn't fork.
    description: "Same change to local and remote",
    parent: {
      "guid": "0b3a72a1bea2",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    local: [{
      "given-name": "Skip",
      "family-name": "Hammond",
    }],
    remote: {
      "guid": "0b3a72a1bea2",
      "version": 1,
      "given-name": "Skip",
      "family-name": "Hammond",
    },
    reconciled: {
      "guid": "0b3a72a1bea2",
      "given-name": "Skip",
      "family-name": "Hammond",
    },
  },
  {
    description: "Conflicting changes to single field",
    parent: {
      // This is what we last wrote to the sync server.
      "guid": "62068784d089",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    local: [{
      // The current version of the local record - the given-name has changed locally.
      "given-name": "Skip",
      "family-name": "Hammond",
    }],
    remote: {
      // An incoming record has a different given-name than any of the above!
      "guid": "62068784d089",
      "version": 1,
      "given-name": "Kip",
      "family-name": "Hammond",
    },
    forked: {
      // So we've forked the local record to a new GUID (and the next sync is
      // going to write this as a new record)
      "given-name": "Skip",
      "family-name": "Hammond",
    },
    reconciled: {
      // And we've updated the local version of the record to be the remote version.
      guid: "62068784d089",
      "given-name": "Kip",
      "family-name": "Hammond",
    },
  },
  {
    description: "Conflicting changes to multiple fields",
    parent: {
      "guid": "244dbb692e94",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "NZ",
    },
    local: [{
      "given-name": "Skip",
      "family-name": "Hammond",
      "country": "AU",
    }],
    remote: {
      "guid": "244dbb692e94",
      "version": 1,
      "given-name": "Kip",
      "family-name": "Hammond",
      "country": "CA",
    },
    forked: {
      "given-name": "Skip",
      "family-name": "Hammond",
      "country": "AU",
    },
    reconciled: {
      "guid": "244dbb692e94",
      "given-name": "Kip",
      "family-name": "Hammond",
      "country": "CA",
    },
  },
  {
    description: "Field deleted locally, changed remotely",
    parent: {
      "guid": "6fc45e03d19a",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "AU",
    },
    local: [{
      "given-name": "Mark",
      "family-name": "Hammond",
    }],
    remote: {
      "guid": "6fc45e03d19a",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "NZ",
    },
    forked: {
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    reconciled: {
      "guid": "6fc45e03d19a",
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "NZ",
    },
  },
  {
    description: "Field changed locally, deleted remotely",
    parent: {
      "guid": "fff9fa27fa18",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "AU",
    },
    local: [{
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "NZ",
    }],
    remote: {
      "guid": "fff9fa27fa18",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
    },
    forked: {
      "given-name": "Mark",
      "family-name": "Hammond",
      "country": "NZ",
    },
    reconciled: {
      "guid": "fff9fa27fa18",
      "given-name": "Mark",
      "family-name": "Hammond",
    },
  },
  {
    // Created, last modified should be synced; last used and times used should
    // be local. Remote created time older than local, remote modified time
    // newer than local.
    description: "Created, last modified time reconciliation without local changes",
    parent: {
      "guid": "5113f329c42f",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "timeCreated": 1234,
      "timeLastModified": 5678,
      "timeLastUsed": 5678,
      "timesUsed": 6,
    },
    local: [],
    remote: {
      "guid": "5113f329c42f",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "timeCreated": 1200,
      "timeLastModified": 5700,
      "timeLastUsed": 5700,
      "timesUsed": 3,
    },
    reconciled: {
      "guid": "5113f329c42f",
      "given-name": "Mark",
      "family-name": "Hammond",
      "timeCreated": 1200,
      "timeLastModified": 5700,
      "timeLastUsed": 5678,
      "timesUsed": 6,
    },
  },
  {
    // Local changes, remote created time newer than local, remote modified time
    // older than local.
    description: "Created, last modified time reconciliation with local changes",
    parent: {
      "guid": "791e5608b80a",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "timeCreated": 1234,
      "timeLastModified": 5678,
      "timeLastUsed": 5678,
      "timesUsed": 6,
    },
    local: [{
      "given-name": "Skip",
      "family-name": "Hammond",
    }],
    remote: {
      "guid": "791e5608b80a",
      "version": 1,
      "given-name": "Mark",
      "family-name": "Hammond",
      "timeCreated": 1300,
      "timeLastModified": 5000,
      "timeLastUsed": 5000,
      "timesUsed": 3,
    },
    reconciled: {
      "guid": "791e5608b80a",
      "given-name": "Skip",
      "family-name": "Hammond",
      "timeCreated": 1234,
      "timeLastUsed": 5678,
      "timesUsed": 6,
    },
  },
];

const CREDIT_CARD_RECONCILE_TESTCASES = [
  {
    description: "Local change",
    parent: {
      // So when we last wrote the record to the server, it had these values.
      "guid": "2bbd2d8fbc6b",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    local: [{
      // The current local record - by comparing against parent we can see that
      // only the cc-number has changed locally.
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    }],
    remote: {
      // This is the incoming record. It has the same values as "parent", so
      // we can deduce the record hasn't actually been changed remotely so we
      // can safely ignore the incoming record and write our local changes.
      "guid": "2bbd2d8fbc6b",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    reconciled: {
      "guid": "2bbd2d8fbc6b",
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    },
  },
  {
    description: "Remote change",
    parent: {
      "guid": "e3680e9f890d",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    }],
    remote: {
      "guid": "e3680e9f890d",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    },
    reconciled: {
      "guid": "e3680e9f890d",
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    },
  },

  {
    description: "New local field",
    parent: {
      "guid": "0cba738b1be0",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    }],
    remote: {
      "guid": "0cba738b1be0",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    reconciled: {
      "guid": "0cba738b1be0",
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
  },
  {
    description: "New remote field",
    parent: {
      "guid": "be3ef97f8285",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    }],
    remote: {
      "guid": "be3ef97f8285",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
    reconciled: {
      "guid": "be3ef97f8285",
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
  },
  {
    description: "Deleted field locally",
    parent: {
      "guid": "9627322248ec",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    }],
    remote: {
      "guid": "9627322248ec",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
    reconciled: {
      "guid": "9627322248ec",
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
  },
  {
    description: "Deleted field remotely",
    parent: {
      "guid": "7d7509f3eeb2",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    }],
    remote: {
      "guid": "7d7509f3eeb2",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    reconciled: {
      "guid": "7d7509f3eeb2",
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
  },
  {
    description: "Local and remote changes to unrelated fields",
    parent: {
      // The last time we wrote this to the server, "cc-exp-month" was 12.
      "guid": "e087a06dfc57",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
    local: [{
      // The current local record - so locally we've changed "cc-number".
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
      "cc-exp-month": 12,
    }],
    remote: {
      // Remotely, we've changed "cc-exp-month" to 1.
      "guid": "e087a06dfc57",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 1,
    },
    reconciled: {
      "guid": "e087a06dfc57",
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
      "cc-exp-month": 1,
    },
  },
  {
    description: "Multiple local changes",
    parent: {
      "guid": "340a078c596f",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    local: [{
      "cc-name": "Skip",
      "cc-number": "1111222233334444",
    }, {
      "cc-name": "Skip",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    }],
    remote: {
      "guid": "340a078c596f",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-year": 2000,
    },
    reconciled: {
      "guid": "340a078c596f",
      "cc-name": "Skip",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
      "cc-exp-year": 2000,
    },
  },
  {
    // Local and remote diverged from the shared parent, but the values are the
    // same, so we shouldn't fork.
    description: "Same change to local and remote",
    parent: {
      "guid": "0b3a72a1bea2",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    }],
    remote: {
      "guid": "0b3a72a1bea2",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    },
    reconciled: {
      "guid": "0b3a72a1bea2",
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    },
  },
  {
    description: "Conflicting changes to single field",
    parent: {
      // This is what we last wrote to the sync server.
      "guid": "62068784d089",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    local: [{
      // The current version of the local record - the cc-number has changed locally.
      "cc-name": "John Doe",
      "cc-number": "1111111111111111",
    }],
    remote: {
      // An incoming record has a different cc-number than any of the above!
      "guid": "62068784d089",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    },
    forked: {
      // So we've forked the local record to a new GUID (and the next sync is
      // going to write this as a new record)
      "cc-name": "John Doe",
      "cc-number": "1111111111111111",
    },
    reconciled: {
      // And we've updated the local version of the record to be the remote version.
      guid: "62068784d089",
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    },
  },
  {
    description: "Conflicting changes to multiple fields",
    parent: {
      "guid": "244dbb692e94",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "1111111111111111",
      "cc-exp-month": 1,
    }],
    remote: {
      "guid": "244dbb692e94",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
      "cc-exp-month": 3,
    },
    forked: {
      "cc-name": "John Doe",
      "cc-number": "1111111111111111",
      "cc-exp-month": 1,
    },
    reconciled: {
      "guid": "244dbb692e94",
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
      "cc-exp-month": 3,
    },
  },
  {
    description: "Field deleted locally, changed remotely",
    parent: {
      "guid": "6fc45e03d19a",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    }],
    remote: {
      "guid": "6fc45e03d19a",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 3,
    },
    forked: {
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    reconciled: {
      "guid": "6fc45e03d19a",
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 3,
    },
  },
  {
    description: "Field changed locally, deleted remotely",
    parent: {
      "guid": "fff9fa27fa18",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 12,
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 3,
    }],
    remote: {
      "guid": "fff9fa27fa18",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
    forked: {
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "cc-exp-month": 3,
    },
    reconciled: {
      "guid": "fff9fa27fa18",
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
    },
  },
  {
    // Created, last modified should be synced; last used and times used should
    // be local. Remote created time older than local, remote modified time
    // newer than local.
    description: "Created, last modified time reconciliation without local changes",
    parent: {
      "guid": "5113f329c42f",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "timeCreated": 1234,
      "timeLastModified": 5678,
      "timeLastUsed": 5678,
      "timesUsed": 6,
    },
    local: [],
    remote: {
      "guid": "5113f329c42f",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "timeCreated": 1200,
      "timeLastModified": 5700,
      "timeLastUsed": 5700,
      "timesUsed": 3,
    },
    reconciled: {
      "guid": "5113f329c42f",
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "timeCreated": 1200,
      "timeLastModified": 5700,
      "timeLastUsed": 5678,
      "timesUsed": 6,
    },
  },
  {
    // Local changes, remote created time newer than local, remote modified time
    // older than local.
    description: "Created, last modified time reconciliation with local changes",
    parent: {
      "guid": "791e5608b80a",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "timeCreated": 1234,
      "timeLastModified": 5678,
      "timeLastUsed": 5678,
      "timesUsed": 6,
    },
    local: [{
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
    }],
    remote: {
      "guid": "791e5608b80a",
      "version": 1,
      "cc-name": "John Doe",
      "cc-number": "1111222233334444",
      "timeCreated": 1300,
      "timeLastModified": 5000,
      "timeLastUsed": 5000,
      "timesUsed": 3,
    },
    reconciled: {
      "guid": "791e5608b80a",
      "cc-name": "John Doe",
      "cc-number": "4444333322221111",
      "timeCreated": 1234,
      "timeLastUsed": 5678,
      "timesUsed": 6,
    },
  },
];

add_task(async function test_reconcile_unknown_version() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);

  // Cross-version reconciliation isn't supported yet. See bug 1377204.
  throws(() => {
    profileStorage.addresses.reconcile({
      "guid": "31d83d2725ec",
      "version": 2,
      "given-name": "Mark",
      "family-name": "Hammond",
    });
  }, /Got unknown record version/);
});

add_task(async function test_reconcile_idempotent() {
  let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME);

  let guid = "de1ba7b094fe";
  profileStorage.addresses.add({
    guid,
    version: 1,
    "given-name": "Mark",
    "family-name": "Hammond",
  }, {sourceSync: true});
  profileStorage.addresses.update(guid, {
    "given-name": "Skip",
    "family-name": "Hammond",
    "organization": "Mozilla",
  });

  let remote = {
    guid,
    "version": 1,
    "given-name": "Mark",
    "family-name": "Hammond",
    "tel": "123456",
  };

  {
    let {forkedGUID} = profileStorage.addresses.reconcile(remote);
    let updatedRecord = profileStorage.addresses.get(guid, {
      rawData: true,
    });

    ok(!forkedGUID, "First merge should not fork record");
    ok(objectMatches(updatedRecord, {
      "guid": "de1ba7b094fe",
      "given-name": "Skip",
      "family-name": "Hammond",
      "organization": "Mozilla",
      "tel": "123456",
    }), "First merge should merge local and remote changes");
  }

  {
    let {forkedGUID} = profileStorage.addresses.reconcile(remote);
    let updatedRecord = profileStorage.addresses.get(guid, {
      rawData: true,
    });

    ok(!forkedGUID, "Second merge should not fork record");
    ok(objectMatches(updatedRecord, {
      "guid": "de1ba7b094fe",
      "given-name": "Skip",
      "family-name": "Hammond",
      "organization": "Mozilla",
      "tel": "123456",
    }), "Second merge should not change record");
  }
});

add_task(async function test_reconcile_three_way_merge() {
  let TESTCASES = {
    addresses: ADDRESS_RECONCILE_TESTCASES,
    creditCards: CREDIT_CARD_RECONCILE_TESTCASES,
  };

  for (let collectionName in TESTCASES) {
    info(`Start to test reconcile on ${collectionName}`);

    let profileStorage = await initProfileStorage(TEST_STORE_FILE_NAME, null, collectionName);

    for (let test of TESTCASES[collectionName]) {
      info(test.description);

      profileStorage[collectionName].add(test.parent, {sourceSync: true});

      for (let updatedRecord of test.local) {
        profileStorage[collectionName].update(test.parent.guid, updatedRecord);
      }

      let localRecord = profileStorage[collectionName].get(test.parent.guid, {
        rawData: true,
      });

      let onReconciled = TestUtils.topicObserved(
        "formautofill-storage-changed",
        (subject, data) =>
          data == "reconcile" &&
          subject.wrappedJSObject.collectionName == collectionName
      );
      let {forkedGUID} = profileStorage[collectionName].reconcile(test.remote);
      await onReconciled;
      let reconciledRecord = profileStorage[collectionName].get(test.parent.guid, {
        rawData: true,
      });
      if (forkedGUID) {
        let forkedRecord = profileStorage[collectionName].get(forkedGUID, {
          rawData: true,
        });

        notEqual(forkedRecord.guid, reconciledRecord.guid);
        equal(forkedRecord.timeLastModified, localRecord.timeLastModified);
        ok(objectMatches(forkedRecord, test.forked),
          `${test.description} should fork record`);
      } else {
        ok(!test.forked, `${test.description} should not fork record`);
      }

      ok(objectMatches(reconciledRecord, test.reconciled));
    }
  }
});
