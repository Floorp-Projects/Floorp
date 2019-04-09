
gcparam('maxNurseryBytes', 2 ** 32 - 1);
gc()

gcparam('minNurseryBytes', 32*1024);
gcparam('maxNurseryBytes', 64*1024);
gc()

