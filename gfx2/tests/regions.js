/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
const SHOW_ALL = false;
const DEBUG = true;

test();

function test()
{
  debug("Region testing - begin\n");
  
  testCreation();

  testSetToRect();

  debug("Region testing - end\n");
}

function testCreation()
{
  debug("Testing region creation - begin\n");

  var region = newRegion();
  assert(region.isEmpty(),
         "testCreation: region = newRegion() should be empty");

  debug("Testing region creation - end\n");
}

function testSetToRect()
{
  debug("Testing region resizing - begin\n");

  var region = newRegion();

  region.setToRect(0, 0, 400, 400);
  assert(!region.isEmpty(),
         "testSetToRect: region.setToRect(0, 0, 400, 400) should not be empty");

  region.init();
  assert(region.isEmpty(),
         "testSetToRect: region.init() should be empty");

  region.setToRect(100, 100, 200, 200);
  assert(!region.isEmpty(),
         "testSetToRect: region.setToRect(100, 100, 200, 200) should not be empty");

  // apparantly this adds a rect, not sets.
  region.setToRect(0, 0, 0, 0);
  assert(region.isEmpty(),
         "testSetToRect: region.setToRect(0, 0, 0, 0) should be empty");

  region.setToRect(0, 0, 200, 200);
  assert(!region.isEmpty(),
         "testSetToRect: region.setToRect(0, 0, 200, 200) should not be empty");

  debug("Testing region resizing - end\n");
}

function newRegion()
{
  const nsIRegion = Components.interfaces.nsIRegion;

  var region = Components.classes["@mozilla.org/gfx/region;2"].createInstance(nsIRegion);
  return region;
}

function debug(message)
{
  if (DEBUG)
    dump(message);
}

function assert(condition, message)
{
  if (SHOW_ALL)
    dump(condition + " : "+message+"\n");

  if (!condition)
    throw message;
}
