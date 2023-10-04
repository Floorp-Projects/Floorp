/**
  * This tests APIs that are relied on by external consumers such as fuzzers,
  * which could be broken if modified with regard only to in-tree code.
  */

// getRealmConfiguration and getBuildConfiguration
// If API changes are planned here, be sure to notify anyone who is fuzzing SM.

config = getRealmConfiguration();

assertEq(typeof config, "object");

for (const [key, value] of Object.entries(config)) {
  assertEq(getRealmConfiguration(key), value);
}

config = getBuildConfiguration();

assertEq(typeof config, "object");

for (const [key, value] of Object.entries(config)) {
  assertEq(getBuildConfiguration(key), value);
}
