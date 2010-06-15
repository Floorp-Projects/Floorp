/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function Elements(arrayLike) {
    this.i = 0;
    this.stop = arrayLike.length;
    this.source = arrayLike;
}
Elements.prototype = {
    // FIXME: use Proxy.iterator when it's available
    __iterator__: function() this,
    next: function() {
        if (this.i >= this.stop)
            throw StopIteration;
        return this.source[this.i++];
    }
};
function elements(arrayLike) {
    return new Elements(arrayLike);
}
