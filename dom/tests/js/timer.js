
function oneShot(testNum, str)
{
  dump("Test #" + testNum + " successful:" + str + "\n");
}

setTimeout(oneShot, 1000, 1, "one shot timer with function argument");
setTimeout("oneShot(2, 'one shot timer with string argument');", 2000);

function reschedule(testNum, numTimes)
{
  if (numTimes == 4) { 
    dump("Test #" + testNum + " successful: Creating a timeout in a timeout\n");
    kickoff4();
  }
  else {
    dump("Test #" + testNum + " in progress: " + numTimes + "\n");
    setTimeout(reschedule, 500, 3, numTimes+1);
  }
}

setTimeout(reschedule, 3000, 3, 0); 

var count = 0;
var repeat_timer = null;
function repeat(testNum, numTimes, str, func, delay)
{
  dump("Test #" + testNum + " in progress: interval delayed by " + delay + " milliseconds\n");
  if (count++ > numTimes) {
    clearInterval(repeat_timer);
    dump("Test #" + testNum + " successful: " + str + "\n");
    if (func != null) {
      func();
    }
  }
}

function kickoff4()
{
  repeat_timer = setInterval(repeat, 500, 4, 5, "interval test", kickoff5);
}

//setTimeout(kickoff4, 5000);

function oneShot2(testNum)
{
  dump("Test #" + testNum + " in progress: one shot timer\n");
  if (count++ == 4) {
    dump("Test #" + testNum + " in progress: end of one shots\n");
  }
  else {
    setTimeout(oneShot2, 500, 5);
  }
}

function kickoff5()
{
  count = 0;
  setTimeout(oneShot2, 500, 5);
  repeat_timer = setInterval("repeat(5, 8, 'multiple timer test', kickoff6)", 600);
}

//setTimeout(kickoff5, 12000);

function kickoff6()
{
  dump("Test #6: Interval timeout should end when you go to a new page\n");
  setInterval(repeat, 1000, 6, 1000, "endless timer test", null);
}

//setTimeout(kickoff6, 18000);