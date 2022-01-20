// |reftest| skip-if(!this.hasOwnProperty("Record"))

const realm = newGlobal();

const realm_record = realm.eval(`Record({ x: 1, y: 2 })`);

assertEq(realm_record === #{ x: 1, y: 2 }, true);

if (typeof reportCompare === "function") reportCompare(0, 0);
