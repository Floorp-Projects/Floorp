// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349331;
var summary = 'generator.close without GeneratorExit';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var catch1, catch2, catch3, finally1, finally2, finally3;
  var iter;

  function gen()
  {
    yield 1;
    try {
      try {
        try {
          yield 1;
        } catch (e) {
          catch1 = true;
        } finally {
          finally1 = true;
        }
      } catch (e) {
        catch2 = true;
      } finally {
        finally2 = true;
      }
    } catch (e) {
      catch3 = true;
    } finally {
      finally3 = true;
    }
  }

// test explicit close call
  catch1 = catch2 = catch3 = finally1 = finally2 = finally3 = false;
  iter = gen();
  iter.next();
  iter.next();
  iter.close();

  var passed = !catch1 && !catch2 && !catch3 && finally1 && finally2 &&
    finally3;

  if (!passed) {
    print("Failed!");
    print("catch1=" + catch1 + " catch2=" + catch2 + " catch3=" +
	  catch3);
    print("finally1=" + finally1 + " finally2=" + finally2 +
	  " finally3=" + finally3);
  }

  reportCompare(true, passed, 'test explicit close call');

// test GC-invoked close
  catch1 = catch2 = catch3 = finally1 = finally2 = finally3 = false;
  iter = gen();
  iter.next();
  iter.next();
  iter = null;
  gc();
  gc();

  var passed = !catch1 && !catch2 && !catch3 && finally1 && finally2 &&
    finally3;

  if (!passed) {
    print("Failed!");
    print("catch1=" + catch1 + " catch2=" + catch2 + " catch3=" +
	  catch3);
    print("finally1=" + finally1 + " finally2=" + finally2 +
	  " finally3="+finally3);
  }
  reportCompare(true, passed, 'test GC-invoke close');

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
