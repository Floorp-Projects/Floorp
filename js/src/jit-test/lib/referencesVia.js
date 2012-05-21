/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function referencesVia(from, edge, to) {
    if (typeof findReferences !== 'function')
        return true;

    edge = "edge: " + edge;
    var edges = findReferences(to);
    if (edge in edges && edges[edge].indexOf(from) != -1)
        return true;

    // Be nice: make it easy to fix if the edge name has just changed.
    var alternatives = [];
    for (var e in edges) {
        if (edges[e].indexOf(from) != -1)
            alternatives.push(e);
    }
    if (alternatives.length == 0) {
        print("referent not referred to by referrer after all");
    } else {
        print("referent is not referenced via: " + uneval(edge));
        print("but it is referenced via:       " + uneval(alternatives));
    }
    print("all incoming edges, from any object:");
    for (var e in edges)
        print(e);
    return false;
}
