
function rainbow(str)
{
  str = String(str);
  var c = str.length;
  var rv = "";

	for (var i = 0; i < c; i++)
    {
	  var color = randomRange (2, 6);
	  rv += unescape ("%03" + color + str[i]);
	}

	return rv;

}

function fade(str) 
{
  var colors = new Array(1, 14, 10, 15, 0);
  var cIndex = 0;
  var msg = "";
  for (i = 0; i < str.length; i++)
  {
	msg += "%03" + colors[cIndex] + str[i];
	if ((++cIndex) == 5)
	{
	  cIndex = 0;
   	}
  }

  return unescape(msg);

 }                                    

