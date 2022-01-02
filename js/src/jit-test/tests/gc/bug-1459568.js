gczeal(0);
gcparam("markStackLimit", 1);
gczeal(18, 1);
grayRoot()[0] = "foo";
grayRoot()[1] = {};
grayRoot().x = 0;
gc();
