/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
function stopMe(){throw Error("boom");}try{stopMe();var a=1;a=a*2;}catch(e){};
//# sourceMappingURL=bogus.map
