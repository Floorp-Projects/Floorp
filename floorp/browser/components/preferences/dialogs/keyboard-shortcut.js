/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let keysPressed = [];
let isVerificationActive = false;

function startVerification() {
  if (!isVerificationActive) {
    isVerificationActive = true;
    keysPressed = [];
    console.log('Verification started');
    document.addEventListener('keydown', handleKeyDown);
    document.addEventListener('keyup', handleKeyUp);
  }
}

function endVerification() {
  if (isVerificationActive) {
    isVerificationActive = false;
    console.log('Verification ended');
    document.removeEventListener('keydown', handleKeyDown);
    document.removeEventListener('keyup', handleKeyUp);
    console.log('Pressed keys:', keysPressed);
  }
}

function handleKeyDown(event) {
  if (isVerificationActive && !keysPressed.includes(event.key)) {
    keysPressed.push(event.key);
  }
}

function handleKeyUp(event) {
  if (isVerificationActive && keysPressed.includes(event.key)) {
    const index = keysPressed.indexOf(event.key);
    keysPressed.splice(index, 1);
  }
}

startVerification();

setTimeout(() => {
  endVerification();
}, 5000);
