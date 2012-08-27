letters = new Array("a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z");
numbers = new Array(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26);
colors  = new Array("FF","CC","99","66","33","00");

var endResult;

function doTest()
{
   endResult = "";

   // make up email address
  /* BEGIN LOOP */
   for (var k=0;k<4000;k++)
   {
      name = makeName(6);
      (k%2)?email=name+"@mac.com":email=name+"(at)mac.com";

      // validate the email address
      var pattern = /^[a-zA-Z0-9\-\._]+@[a-zA-Z0-9\-_]+(\.?[a-zA-Z0-9\-_]*)\.[a-zA-Z]{2,3}$/;

      if(pattern.test(email))
      {
         var r = email + " appears to be a valid email address.";
         addResult(r);
      }
      else
      {
         r = email + " does NOT appear to be a valid email address.";
         addResult(r);
      }
   }
  /* END LOOP */

   // make up ZIP codes
  /* BEGIN LOOP */
   for (var s=0;s<4000;s++)
   {
      var zipGood = true;
      var zip = makeNumber(4);
      (s%2)?zip=zip+"xyz":zip=zip.concat("7");

      // validate the zip code
  /* BEGIN LOOP */
      for (var i = 0; i < zip.length; i++) {
          var ch = zip.charAt(i);
          if (ch < "0" || ch > "9") {
              zipGood = false;
              r = zip + " contains letters.";
              addResult(r);
          }
      }
  /* END LOOP */
      if (zipGood && zip.length>5)
      {
         zipGood = false;
         r = zip + " is longer than five characters.";
         addResult(r);
      }
      if (zipGood)
      {
         r = zip + " appears to be a valid ZIP code.";
         addResult(r);
      }
   }
  /* END LOOP */
}

function makeName(n)
{
   var tmp = "";
  /* BEGIN LOOP */
   for (var i=0;i<n;i++)
   {
      var l = Math.floor(26*Math.random());
      tmp += letters[l];
   }
  /* END LOOP */
   return tmp;
}

function makeNumber(n)
{
   var tmp = "";
  /* BEGIN LOOP */
   for (var i=0;i<n;i++)
   {
      var l = Math.floor(9*Math.random());
      tmp = tmp.concat(l);
   }
  /* END LOOP */
   return tmp;
}

function addResult(r)
{
   endResult += "\n" + r;
}

doTest();
