/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * NB: this test hardcodes the value of PropertyTree::MAX_HEIGHT.
 */
var i = 0;
function add_p0to127(o) {
  o.p00 = ++i;o.p01 = ++i;o.p02 = ++i;o.p03 = ++i;o.p04 = ++i;o.p05 = ++i;o.p06 = ++i;o.p07 = ++i;
  o.p10 = ++i;o.p11 = ++i;o.p12 = ++i;o.p13 = ++i;o.p14 = ++i;o.p15 = ++i;o.p16 = ++i;o.p17 = ++i;
  o.p20 = ++i;o.p21 = ++i;o.p22 = ++i;o.p23 = ++i;o.p24 = ++i;o.p25 = ++i;o.p26 = ++i;o.p27 = ++i;
  o.p30 = ++i;o.p31 = ++i;o.p32 = ++i;o.p33 = ++i;o.p34 = ++i;o.p35 = ++i;o.p36 = ++i;o.p37 = ++i;
  o.p40 = ++i;o.p41 = ++i;o.p42 = ++i;o.p43 = ++i;o.p44 = ++i;o.p45 = ++i;o.p46 = ++i;o.p47 = ++i;
  o.p50 = ++i;o.p51 = ++i;o.p52 = ++i;o.p53 = ++i;o.p54 = ++i;o.p55 = ++i;o.p56 = ++i;o.p57 = ++i;
  o.p60 = ++i;o.p61 = ++i;o.p62 = ++i;o.p63 = ++i;o.p64 = ++i;o.p65 = ++i;o.p66 = ++i;o.p67 = ++i;
  o.p70 = ++i;o.p71 = ++i;o.p72 = ++i;o.p73 = ++i;o.p74 = ++i;o.p75 = ++i;o.p76 = ++i;o.p77 = ++i;
  o.p80 = ++i;o.p81 = ++i;o.p82 = ++i;o.p83 = ++i;o.p84 = ++i;o.p85 = ++i;o.p86 = ++i;o.p87 = ++i;
  o.p90 = ++i;o.p91 = ++i;o.p92 = ++i;o.p93 = ++i;o.p94 = ++i;o.p95 = ++i;o.p96 = ++i;o.p97 = ++i;
  o.p100= ++i;o.p101= ++i;o.p102= ++i;o.p103= ++i;o.p104= ++i;o.p105= ++i;o.p106= ++i;o.p107= ++i;
  o.p110= ++i;o.p111= ++i;o.p112= ++i;o.p113= ++i;o.p114= ++i;o.p115= ++i;o.p116= ++i;o.p117= ++i;
  o.p120= ++i;o.p121= ++i;o.p122= ++i;o.p123= ++i;o.p124= ++i;o.p125= ++i;o.p126= ++i;o.p127= ++i;
  o.p130= ++i;o.p131= ++i;o.p132= ++i;o.p133= ++i;o.p134= ++i;o.p135= ++i;o.p136= ++i;o.p137= ++i;
  o.p140= ++i;o.p141= ++i;o.p142= ++i;o.p143= ++i;o.p144= ++i;o.p145= ++i;o.p146= ++i;o.p147= ++i;
  o.p150= ++i;o.p151= ++i;o.p152= ++i;o.p153= ++i;o.p154= ++i;o.p155= ++i;o.p156= ++i;o.p157= ++i;
  return o;
}
function add_p128(o) {
  o.p200 = ++i;
}
var oarr = [];
for (var i = 0; i < 2; i++)
  oarr[i] = {};
var o = add_p0to127(oarr[0]);
var o2 = add_p0to127(oarr[1]);
var o_shape127 = shapeOf(o);
assertEq(o_shape127, shapeOf(o2));
add_p128(o);
add_p128(o2);
var o_shape128 = shapeOf(o);
assertEq(false, o_shape128 === shapeOf(o2));
delete o.p200;
assertEq(false, shapeOf(o) === o_shape127);
add_p128(o);
assertEq(false, shapeOf(o) === o_shape128);

reportCompare(true, true, "don't crash");
