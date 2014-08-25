/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;

SpecialPowers.addPermission("mobileconnection", true, document);

const WHT = 0xFFFFFFFF;
const BLK = 0x000000FF;
const RED = 0xFF0000FF;
const GRN = 0x00FF00FF;
const BLU = 0x0000FFFF;
const TSP = 0;

let iccManager = navigator.mozIccManager;
ok(iccManager instanceof MozIccManager,
   "iccManager is instanceof " + iccManager.constructor);

// TODO: Bug 932650 - B2G RIL: WebIccManager API - add marionette tests for
//                    multi-sim
// In single sim scenario, there is only one sim card, we can use below way to
// check iccId and get icc object. But in multi-sim, the index of iccIds may
// not map to sim slot directly, we should have a better way to handle this.
let iccIds = iccManager.iccIds;
ok(Array.isArray(iccIds), "iccIds is an array");
ok(iccIds.length > 0, "iccIds.length is " + iccIds.length);

let iccId = iccIds[0];
is(iccId, "89014103211118510720", "iccId is " + iccId);

let icc = iccManager.getIccById(iccId);
ok(icc instanceof MozIcc, "icc is instanceof " + icc.constructor);


// Basic Image, see record number 1 in EFimg.
let basicIcon = {
  width: 8,
  height: 8,
  codingScheme: "basic",
  pixels: [WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT,
           BLK, BLK, BLK, BLK, BLK, BLK, WHT, WHT,
           WHT, BLK, WHT, BLK, BLK, WHT, BLK, WHT,
           WHT, BLK, BLK, WHT, WHT, BLK, BLK, WHT,
           WHT, BLK, BLK, WHT, WHT, BLK, BLK, WHT,
           WHT, BLK, WHT, BLK, BLK, WHT, BLK, WHT,
           WHT, WHT, BLK, BLK, BLK, BLK, WHT, WHT,
           WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT]
};
// Color Image, see record number 3 in EFimg.
let colorIcon = {
  width: 8,
  height: 8,
  codingScheme: "color",
  pixels: [BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU,
           BLU, RED, RED, RED, RED, RED, RED, BLU,
           BLU, RED, GRN, GRN, GRN, RED, RED, BLU,
           BLU, RED, RED, GRN, GRN, RED, RED, BLU,
           BLU, RED, RED, GRN, GRN, RED, RED, BLU,
           BLU, RED, RED, GRN, GRN, GRN, RED, BLU,
           BLU, RED, RED, RED, RED, RED, RED, BLU,
           BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU]
};
// Color Image with Transparency, see record number 5 in EFimg.
let colorTransparencyIcon = {
  width: 8,
  height: 8,
  codingScheme: "color-transparency",
  pixels: [TSP, TSP, TSP, TSP, TSP, TSP, TSP, TSP,
           TSP, RED, RED, RED, RED, RED, RED, TSP,
           TSP, RED, GRN, GRN, GRN, RED, RED, TSP,
           TSP, RED, RED, GRN, GRN, RED, RED, TSP,
           TSP, RED, RED, GRN, GRN, RED, RED, TSP,
           TSP, RED, RED, GRN, GRN, GRN, RED, TSP,
           TSP, RED, RED, RED, RED, RED, RED, TSP,
           TSP, TSP, TSP, TSP, TSP, TSP, TSP, TSP]
};

function isIcons(icons, expectedIcons, message) {
  is(icons.length, expectedIcons.length, message);
  for (let i = 0; i < icons.length; i++) {
    let icon = icons[i];
    let expectedIcon = expectedIcons[i];

    is(icon.width, expectedIcon.width, message);
    is(icon.height, expectedIcon.height, message);
    is(icon.codingScheme, expectedIcon.codingScheme, message);

    is(icon.pixels.length, expectedIcon.pixels.length);
    for (let j = 0; j < icon.pixels.length; j++) {
      is(icon.pixels[j], expectedIcon.pixels[j], message);
    }
  }
}

let pendingEmulatorCmdCount = 0;
function sendStkPduToEmulator(command, func, expect) {
  ++pendingEmulatorCmdCount;

  runEmulatorCmd(command, function(result) {
    --pendingEmulatorCmdCount;
    is(result[0], "OK");
  });

  icc.onstkcommand = function(evt) {
    if (expect) {
      func(evt.command, expect);
    } else {
      func(evt.command);
    }
  }
}

function runNextTest() {
  let test = tests.pop();
  if (!test) {
    cleanUp();
    return;
  }

  let command = "stk pdu " + test.command;
  sendStkPduToEmulator(command, test.func, test.expect);
}

function cleanUp() {
  if (pendingEmulatorCmdCount) {
    window.setTimeout(cleanUp, 100);
    return;
  }

  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}
