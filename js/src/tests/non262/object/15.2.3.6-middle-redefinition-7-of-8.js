// |reftest| slow skip-if(!xulRuntime.shell) -- uses shell load() function
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

loadRelativeToScript("defineProperty-setup.js");
runNonTerminalPropertyPresentTestsFraction(7, 8);
