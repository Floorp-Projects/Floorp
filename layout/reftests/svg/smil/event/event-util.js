// Allows a moment for events to be processed then performs a seek and runs
// a snapshot.
function delayedSnapshot(seekTimeInSeconds)
{
  // Allow time for events to be processed
  window.setTimeout(finish, 10, seekTimeInSeconds);
}

function finish(seekTimeInSeconds)
{
  document.documentElement.pauseAnimations();
  if (seekTimeInSeconds)
    document.documentElement.setCurrentTime(seekTimeInSeconds);
  document.documentElement.removeAttribute("class");
}

function click(targetId)
{
  var evt = document.createEvent("MouseEvents");
  evt.initMouseEvent("click", true, true, window,
    0, 0, 0, 0, 0, false, false, false, false, 0, null);
  var target = document.getElementById(targetId);
  target.dispatchEvent(evt);
}

function keypress(charCode)
{
  var evt = document.createEvent("KeyboardEvent");
  evt.initKeyEvent("keypress", true, true, window,
                   false, // ctrlKeyArg
                   false, // altKeyArg
                   false, // shiftKeyArg
                   false, // metaKeyArg
                   0,     // keyCode
                   charCode);
  document.documentElement.dispatchEvent(evt);
}
