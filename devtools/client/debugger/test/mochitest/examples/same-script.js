/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function sameSourceDistinctThread() {
  console.log("same source distinct thread");
}

addEventListener("click", sameSourceDistinctThread);
addEventListener("message", sameSourceDistinctThread);
