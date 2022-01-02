/* Don't trip bogus assert. */

function e()
{
    try
    {
        var t = undefined;
    }
    catch (e)
    {
        var t = null;
    }
    while (t && (t.tagName.toUpperCase() != "BODY"))
        continue;
}
for (var i = 0; i < 20; i++)
  e();
