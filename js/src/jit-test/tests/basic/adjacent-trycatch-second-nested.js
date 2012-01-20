try { }
catch (e) { }

try { throw 2; }
catch (e)
{
  try { throw 3; }
  catch (e2) { }
}
