/**
 * Platform-specific failures not included in the main currentStatus.js list.
 */
var platformFailures;
if (navigator.appVersion.includes("Android")) {
  platformFailures = {
    "value": {},
    "select": {
      "S-Proposed-SM:m.f.w_TEXT-th_SC-1-dM": true,
      "S-Proposed-SM:m.f.w_TEXT-th_SC-1-body": true,
      "S-Proposed-SM:m.f.w_TEXT-th_SC-1-div": true,
      "S-Proposed-SM:m.f.w_TEXT-th_SC-2-dM": true,
      "S-Proposed-SM:m.f.w_TEXT-th_SC-2-body": true,
      "S-Proposed-SM:m.f.w_TEXT-th_SC-2-div": true,
      "S-Proposed-SM:m.b.w_TEXT-th_SC-1-dM": true,
      "S-Proposed-SM:m.b.w_TEXT-th_SC-1-body": true,
      "S-Proposed-SM:m.b.w_TEXT-th_SC-1-div": true,
      "S-Proposed-SM:m.b.w_TEXT-th_SC-2-dM": true,
      "S-Proposed-SM:m.b.w_TEXT-th_SC-2-body": true,
      "S-Proposed-SM:m.b.w_TEXT-th_SC-2-div": true
    }
  }
} else {
  platformFailures = {
    "value": {},
    "select": {}
  }
}
