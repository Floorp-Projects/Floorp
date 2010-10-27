function testRegExpLiteral()
{
  var a;
  for (var i = 0; i < 15; i++)
    a = /foobar/;
}

testRegExpLiteral();

checkStats({
             recorderStarted: 1,
             sideExitIntoInterpreter: 1,
             recorderAborted: 0,
             traceTriggered: 1,
             traceCompleted: 1
           });

