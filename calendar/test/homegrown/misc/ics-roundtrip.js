var eventClass = C.classes["@mozilla.org/calendar/event;1"];
var eventIID = C.interfaces.calIEvent;

dump("* Creating event.\n");
var e = eventClass.createInstance(eventIID);

var ics_xmas = 
'BEGIN:VCALENDAR\nPRODID:-//ORACLE//NONSGML CSDK 9.0.5 - CalDAVServlet 9.0.5//EN\nVERSION:2.0\nBEGIN:VEVENT\nUID:20041119T052239Z-1000472-1-5c0746bb-Oracle\nORGANIZER;X-ORACLE-GUID=E9359406791C763EE0305794071A39A4;CN=Simon Vaillan\n court:mailto:simon.vaillancourt@oracle.com\nSEQUENCE:0\nDTSTAMP:20041124T010028Z\nCREATED:20041119T052239Z\nX-ORACLE-EVENTINSTANCE-GUID:I1+16778354+1+1+438153759\nX-ORACLE-EVENT-GUID:E1+16778354+1+438153759\nX-ORACLE-EVENTTYPE:DAY EVENT\nTRANSP:TRANSPARENT\nSUMMARY:Christmas\nSTATUS:CONFIRMED\nPRIORITY:0\nDTSTART;VALUE=DATE:20041125\nDTEND;VALUE=DATE:20041125\nCLASS:PUBLIC\nATTENDEE;X-ORACLE-GUID=E92F51FB4A48E91CE0305794071A149C;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=James Stevens;PARTSTAT=NEEDS-ACTION:mailto:james.stevens@o\n racle.com\nATTENDEE;X-ORACLE-GUID=E9359406791C763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=FALSE;CN=Simon Vaillancourt;PARTSTAT=ACCEPTED:mailto:simon.vaillan\n court@oracle.com\nATTENDEE;X-ORACLE-GUID=E9359406791D763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Bernard Desruisseaux;PARTSTAT=NEEDS-ACTION:mailto:bernard.\n desruisseaux@oracle.com\nATTENDEE;X-ORACLE-GUID=E9359406791E763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Mario Bonin;PARTSTAT=NEEDS-ACTION:mailto:mario.bonin@oracl\n e.com\nATTENDEE;X-ORACLE-GUID=E9359406791F763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Jeremy Chone;PARTSTAT=NEEDS-ACTION:mailto:jeremy.chone@ora\n cle.com\nATTENDEE;X-ORACLE-PERSONAL-COMMENT-ISDIRTY=TRUE;X-ORACLE-GUID=E9359406792\n 0763EE0305794071A39A4;CUTYPE=INDIVIDUAL;RSVP=TRUE;CN=Mike Shaver;PARTSTA\n T=NEEDS-ACTION:mailto:mike.x.shaver@oracle.com\nATTENDEE;X-ORACLE-GUID=E93594067921763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=David Ball;PARTSTAT=NEEDS-ACTION:mailto:david.ball@oracle.\n com\nATTENDEE;X-ORACLE-GUID=E93594067922763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Marten Haring;PARTSTAT=NEEDS-ACTION:mailto:marten.den.hari\n ng@oracle.com\nATTENDEE;X-ORACLE-GUID=E93594067923763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Peter Egyed;PARTSTAT=NEEDS-ACTION:mailto:peter.egyed@oracl\n e.com\nATTENDEE;X-ORACLE-GUID=E93594067924763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Francois Perrault;PARTSTAT=NEEDS-ACTION:mailto:francois.pe\n rrault@oracle.com\nATTENDEE;X-ORACLE-GUID=E93594067925763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Vladimir Vukicevic;PARTSTAT=NEEDS-ACTION:mailto:vladimir.v\n ukicevic@oracle.com\nATTENDEE;X-ORACLE-GUID=E93594067926763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Cyrus Daboo;PARTSTAT=NEEDS-ACTION:mailto:daboo@isamet.com\nATTENDEE;X-ORACLE-GUID=E93594067927763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Lisa Dusseault;PARTSTAT=NEEDS-ACTION:mailto:lisa@osafounda\n tion.org\nATTENDEE;X-ORACLE-GUID=E93594067928763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Dan Mosedale;PARTSTAT=NEEDS-ACTION:mailto:dan.mosedale@ora\n cle.com\nATTENDEE;X-ORACLE-GUID=E93594067929763EE0305794071A39A4;CUTYPE=INDIVIDUAL\n ;RSVP=TRUE;CN=Stuart Parmenter;PARTSTAT=NEEDS-ACTION:mailto:stuart.parme\n nter@oracle.com\nEND:VEVENT\nEND:VCALENDAR\n\n';

dump("* Setting ical string (xmas)\n");
e.icalString = ics_xmas;
dump("* Checking basic properties\n");
var expectedProps =
  [["title", "Christmas"],
   ["id", "20041119T052239Z-1000472-1-5c0746bb-Oracle"],
   ["priority", 0],
   ["status", "CONFIRMED"],
   ["generation", 0],
   ["isAllDay", true]];
function checkProps(expectedProps, obj) {
  for (var i = 0; i < expectedProps.length; i++) {
    if (obj[expectedProps[i][0]] != expectedProps[i][1]) {
      throw "FAILED! expected " + expectedProps[i][0] + " to be " +
        expectedProps[i][1] + " but got " + obj[expectedProps[i][0]];
    }
  }
}

checkProps(expectedProps, e);

dump("* Checking start date\n");
expectedProps =
  [["month", 10],
   ["day", 25],
   ["year", 2004],
   ["isDate", true]];
checkProps(expectedProps, e.startDate);
dump("* Checking end date\n");
checkProps(expectedProps, e.endDate);

dump("PASSED!\n");
