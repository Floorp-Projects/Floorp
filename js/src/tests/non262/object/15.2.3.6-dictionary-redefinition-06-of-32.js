// |reftest| slow skip-if(!xulRuntime.shell) -- uses shell load() function
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

loadRelativeToScript("defineProperty-setup.js");
runDictionaryPropertyPresentTestsFraction(6, 32);
