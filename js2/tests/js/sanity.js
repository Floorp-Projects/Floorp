class BaseClass {
    
    static var gBC = 12;

    var mBC;

    function fBC(a)
    {
        return mBC + a;
    }

    function get Q() { return 13; }
    function set Q(a) { gBC = 99 + a; }

    constructor function makeBC()
    {
        mBC = 17;
    }

}

var badTest = "";
var test = 1;

var bc:BaseClass = BaseClass.makeBC();

if (BaseClass.gBC != 12) badTest += test + " ";
BaseClass.gBC = 0;

test++; //2
if ((bc + 3) != 20) badTest += test + " ";
test++; //3
if (BaseClass.gBC != 1) badTest += test + " ";

class Extended extends BaseClass {

    var mEx;
    var t = 49200;

    function Extended() 
    {
        mEx = 2;
    }

}


var ex:Extended = new Extended;
test++; //4
if (ex.t != 49200) badTest += test + " ";
test++; //5
if ((ex + bc) != 19) badTest += test + " ";

function loopy(a)
{
    var x = 0;
    foo: 
        while (a > 0) {            
            --a;
            x++;
            if (x == 3) break foo;
        }
        
    return x;
}


test++; //6
if (loopy(17) != 3) badTest += test + " ";

BaseClass.gBC *= ex.mEx;
test++; //7
if (BaseClass.gBC != 2) badTest += test + " ";

var a = 3, b = 2;
a &&= b;
test++; //8
if (a != 2) badTest += test + " ";

test++; //9
if (bc.Q != 13) badTest += test + " ";
bc.Q = 1;
test++; //10
if (BaseClass.gBC != 100) badTest += test + " ";


var cnX = 'X'
var s = '';
function f(n) { var ret = ''; for (var i = 0; i < n; i++) { ret += cnX; } return ret; }
s = f(5);
test++; //11
if (s != 'XXXXX') badTest += test + " ";

var t = "abcdeXXXXXghij";
test++; //12
if (t.split('XXXXX').length != 2) badTest += test + " ";


function x()
{
    var a = 44; 
    throw('frisbee');
}

function ZZ(b)
{
    var a = 12;
    var c = b * 5;
    try {
        x();
        test++; //13    <-- shouldn't execute
        badTest += test + " ";
    }
    catch (e) {
        test++; //13
        if (a != 12) badTest += test + " ";
        test++; //14
        if (b != c / 5) badTest += test + " ";
        test++; //15
        if (e != "frisbee") badTest += test + " ";
    }
}
ZZ(6);

function sw(t) 
{
    var result = "0";
    switch (t) {

        case 1:
            result += "1";
        case 2:
            result += "2";
        default:
            result += "d";
            break;
        case 4:
            result += "4";
            break;
    }
    return result;
}
test++; //16
if (sw(2) != "02d") badTest += test + " ";


class A { static var x = 2; var y; }
class B extends A { static var i = 4; var j; }

test++; //17
if (A.x != 2) badTest += test + " ";

test++; //18
if (B.x != 2) badTest += test + " ";

test++; //19
if (B.i != 4) badTest += test + " ";

var sNAME_UNINITIALIZED:String = '(NO NAME HAS BEEN ASSIGNED)';
var sNAME_MOTHER:String = 'Bessie';
var sNAME_FATHER:String = 'Red';
var sNAME_CHILD:String = 'Junior';


class Cow
{
  static var count:Integer = 0;
  var name:String = sNAME_UNINITIALIZED;

  constructor function Cow(sName:String)
  {
    count++;
    name = sName;
  }
}


class Calf extends Cow
{
  var mother:Cow;
  var father:Cow;

  constructor function Calf(sName:String, objMother:Cow, objFather:Cow)
  {
    Cow(sName);
    mother = objMother;
    father = objFather;
  }
}


var cowMOTHER = new Cow(sNAME_MOTHER);
var cowFATHER = new Cow(sNAME_FATHER);
var cowCHILD = new Calf(sNAME_CHILD, cowMOTHER, cowFATHER);

test++; //20
if (cowCHILD.name != "Junior") badTest += test + " ";
test++; //21
if (cowCHILD.father.name != "Red") badTest += test + " ";

test++; //22
if (this.toString() != "[object Object]") badTest += test + " ";


function f1(a, b = 3, ...f, named c = 4, named d = 5, named e = 6) { return a + b + c + d + e + f.g; }
test++; //23
if (f1(2, "e":1, "c":5, "g":-16) != 0)  badTest += test + " ";

import MentalState, exclude(lunacy);

test++; //24

if (!Normal::functioning)  badTest += test + " ";

var ok:Boolean = false;

try {

    eval("lunacy");

}

catch (x) {

    ok = true;

}

test++; //25

if (!ok)  badTest += test + " ";


if (badTest == 0) print("still sane (after " + test + " tests)") else print("gone off the deep end at test #" + badTest);
