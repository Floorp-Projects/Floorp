function zero(...rest)
{
  assertEq(rest.length, 0, "zero rest wrong length");
}

function tzero()
{
  zero();
}

tzero(); tzero(); tzero();

function one(...rest)
{
  assertEq(rest.length, 1, "one rest wrong length");
}

function tone()
{
  one(0);
}

tone(); tone(); tone();

function two(...rest)
{
  assertEq(rest.length, 2, "two rest wrong length");
}

function ttwo()
{
  two(0, 1);
}

ttwo(); ttwo(); ttwo();

function zeroWithLeading0(x, ...rest)
{
  assertEq(rest.length, 0, "zeroWithLeading0 rest wrong length");
}

function tzeroWithLeading0()
{
  zeroWithLeading0();
}

tzeroWithLeading0(); tzeroWithLeading0(); tzeroWithLeading0();

function zeroWithLeading1(x, ...rest)
{
  assertEq(rest.length, 0, "zeroWithLeading1 rest wrong length");
}

function tzeroWithLeading1()
{
  zeroWithLeading1(0);
}

tzeroWithLeading1(); tzeroWithLeading1(); tzeroWithLeading1();

function oneWithLeading(x, ...rest)
{
  assertEq(rest.length, 1, "oneWithLeading rest wrong length");
}

function toneWithLeading()
{
  oneWithLeading(0, 1);
}

toneWithLeading(); toneWithLeading(); toneWithLeading();

function twoWithLeading(x, ...rest)
{
  assertEq(rest.length, 2, "twoWithLeading rest wrong length");
}

function ttwoWithLeading()
{
  twoWithLeading(0, 1, 2);
}

ttwoWithLeading(); ttwoWithLeading(); ttwoWithLeading();
