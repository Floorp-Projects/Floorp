"use strict";

// Tests that JS iterators are automatically wrapped into
// equivalent nsISimpleEnumerator objects.

const Variant = Components.Constructor("@mozilla.org/variant;1",
                                       "nsIWritableVariant",
                                       "setFromVariant");
const SupportsInterfacePointer = Components.Constructor(
  "@mozilla.org/supports-interface-pointer;1", "nsISupportsInterfacePointer");

function wrapEnumerator1(iter) {
  var ip = SupportsInterfacePointer();
  ip.data = iter;
  return ip.data.QueryInterface(Ci.nsISimpleEnumerator);
}

function wrapEnumerator2(iter) {
  var ip = SupportsInterfacePointer();
  ip.data = {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIFilePicker]),
    get files() {
      return iter;
    },
  };
  return ip.data.QueryInterface(Ci.nsIFilePicker).files;
}


function enumToArray(iter) {
  let result = [];
  while (iter.hasMoreElements()) {
    result.push(iter.getNext().QueryInterface(Ci.nsIVariant));
  }
  return result;
}

add_task(async function test_wrapped_js_enumerator() {
  let array = [1, 2, 3, 4];

  for (let wrapEnumerator of [wrapEnumerator1, wrapEnumerator2]) {
    // Test a plain JS iterator. This should automatically be wrapped into
    // an equivalent nsISimpleEnumerator.
    {
      let iter = wrapEnumerator(array.values());
      let result = enumToArray(iter);

      deepEqual(result, array, "Got correct result");
    }

    // Test an object with a QueryInterface method, which implements
    // nsISimpleEnumerator. This should be wrapped and used directly.
    {
      let obj = {
        QueryInterface: ChromeUtils.generateQI(["nsISimpleEnumerator"]),
        _idx: 0,
        hasMoreElements() {
          return this._idx < array.length;
        },
        getNext() {
          return Variant(array[this._idx++]);
        },
      };

      let iter = wrapEnumerator(obj);
      let result = enumToArray(iter);

      deepEqual(result, array, "Got correct result");
    }
  }
});
