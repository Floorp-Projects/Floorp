// |jit-test| error: TypeError;
var TZ_DIFF = getTimeZoneDiff();
var now = new Date;
var TZ_DIFF = getTimeZoneDiff();
var now = new Date;
var MAX_UNIX_TIMET = 2145859200;
var RANGE_EXPANSION_AMOUNT = 60;
function tzOffsetFromUnixTimestamp(timestamp) {
    new Date
  }
function clearDSTOffsetCache(undesiredTimestamp) {
    tzOffsetFromUnixTimestamp()
    tzOffsetFromUnixTimestamp()
    tzOffsetFromUnixTimestamp()
    tzOffsetFromUnixTimestamp()
    tzOffsetFromUnixTimestamp()
  }
function computeCanonicalTZOffset(timestamp) {
    clearDSTOffsetCache()
    tzOffsetFromUnixTimestamp()
  }
var TEST_TIMESTAMPS_SECONDS = [
     0,
     RANGE_EXPANSION_AMOUNT,
     MAX_UNIX_TIMET,
    ];
var TEST_TIMESTAMPS = TEST_TIMESTAMPS_SECONDS.map(function(v) {  });
var CORRECT_TZOFFSETS = TEST_TIMESTAMPS.map(computeCanonicalTZOffset);
var TZ_DIFF = getTimeZoneDiff();
var now = new Date;
var TZ_DIFF = getTimeZoneDiff();
var now = new Date;function getTimeZoneDiff() {
  new Date/60
}
function check(b, desc) {
    function classOf(obj) {
        return Object.prototype.toString.call(obj);
    }
    function ownProperties(obj) {
        return Object.getOwnPropertyNames(obj).
            map(function (p) { return [p, Object.getOwnPropertyDescriptor(obj, p)]; });
    }
    function isCloneable(pair) {    }
    function assertIsCloneOf(a, b, path) {
        ca = classOf(a)
        assertEq(ca, classOf(b), path)
        assertEq(Object.getPrototypeOf(a), ca == "[object Object]" ? Object.prototype : Array.prototype, path)
        pb = ownProperties(b).filter(isCloneable)
        pa = ownProperties(a)
        function byName(a, b) 0
        byName
        (pa.length, pb.length, "should see the same number of properties " + path)
        for (var i = 0; i < pa.length; i++) {
                gczeal(4)
        }
    }
    banner = desc || uneval()
    a = deserialize(serialize(b))
    var queue = [[a, b, banner]];
    while (queue.length) {
        var triple = queue.shift();
        assertIsCloneOf(triple[0], triple[1], triple[2])
    }
}
check({x: 0.7, p: "forty-two", y: null, z: undefined});
check(Object.prototype);
b=[, , 2, 3];
b.expando=true;
b[5]=5;
b[0]=0;b[4]=4;
check(b)([, , , , , , 6])
