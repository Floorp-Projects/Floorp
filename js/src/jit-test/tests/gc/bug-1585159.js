
// Set a nursery size larger than the current nursery size.
gcparam("minNurseryBytes", 1024 * 1024);
gcparam("maxBytes", gcparam("gcBytes") + 4096);

// Allocate something in the nursery.
let obj = {'foo': 38, 'bar': "A string"};

// Trigger a last-ditch GC.
print(gc(0, "last-ditch"));

