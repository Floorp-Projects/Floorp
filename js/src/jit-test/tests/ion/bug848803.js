var JSIL = {};
JSIL.TypeNameParseState = function ()
{
    this.input = 15;
    this.pos = 0
};
JSIL.TypeNameParseState.prototype.substr = function (e)
{
    return e;
};
JSIL.TypeNameParseState.prototype.moveNext = function ()
{
    this.pos += 1;
    return this.pos < this.input;
};
JSIL.TypeNameParseResult = function () {};
JSIL.ParseTypeNameImpl = function (n)
{
    var i = new JSIL.TypeNameParseState()
    var u = new JSIL.TypeNameParseResult;
    while (i.moveNext())
    {
        if (n)
        {
            while (true)
              u.assembly = 1
        }
        u.assembly = i.substr(i.pos + 1);
    }
    return u
};

var u = JSIL.ParseTypeNameImpl(false)
assertEq(u.assembly, 15)
