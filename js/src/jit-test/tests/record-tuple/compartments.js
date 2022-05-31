// |jit-test| --more-compartments; skip-if: !this.hasOwnProperty("Record")
const realm = newGlobal();

const realm_record = realm.eval(`Record({ x: 1, y: 2 })`);

assertEq(realm_record === #{ x: 1, y: 2 }, true);

const realm_tuple = realm.eval(`Tuple(1, 2, 3)`);

assertEq(realm_tuple === #[1, 2, 3], true);

// Test that an object can point to a record in a different realm
const realm2 = newGlobal();

const realm2_object = realm.eval(`new Object()`);

realm2_object['r'] = realm_record;

assertEq(realm2_object['r'] === #{"x": 1, "y": 2}, true);
