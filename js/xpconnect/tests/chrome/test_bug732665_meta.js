var bottom = stackPointerInfo();
var top = bottom;

function nearNativeStackLimit() {
  function inner() {
    try {
      with ({}) { // keep things predictable -- stay in the interpreter
        top = stackPointerInfo();
        var stepsFromLimit = eval("inner()"); // Use eval to force a number of native stackframes to be created.
      }
      return stepsFromLimit + 1;
    } catch(e) {
      // It would be nice to check here that the exception is actually an
      // over-recursion here. But doing so would require toString()ing the
      // exception, which we may not have the stack space to do.
      return 1;
    }
  }
  return inner();
}

var nbFrames = nearNativeStackLimit();
var frameSize = bottom - top;
print("Max stack size:", frameSize, "bytes",
      "\nMaximum number of frames:", nbFrames,
      "\nAverage frame size:", Math.ceil(frameSize / nbFrames), "bytes");
