gczeal(0);
setMarkStackLimit(1);
gczeal(18, 1);
grayRoot()[0] = "foo";
grayRoot()[1] = {};
grayRoot().x = 0;
gc();
