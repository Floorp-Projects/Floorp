/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::cell::RefCell;
use std::collections::{ HashMap, HashSet };
use std::rc::Rc;

/// A node in the reference graph
struct ReferenceGraphNode {
    /// True if this node is used.
    used: RefCell<bool>,

    /// The set of method names which node uses.
    edges: HashSet<Rc<String>>,
}
impl ReferenceGraphNode {
    fn new(edges: HashSet<Rc<String>>) -> Self {
        ReferenceGraphNode {
            used: RefCell::new(false),
            edges,
        }
    }
}

/// A reference graph of the method call.
///
/// Each auto-generated method has corresponding node in this reference graph,
/// and the method definition/declaration are written to the file only if the
/// method is marked as used.
///
/// This is necessary for the following reason:
///   * we generate parseX and parseInterfaceX for `interface X`
///   * parseX is not used if `interface X` appears only in sum interface
pub struct ReferenceGraph {
    /// The map from the node name to node struct.
    /// Node name is the method name without leading "parse".
    refnodes: HashMap<Rc<String>, Rc<ReferenceGraphNode>>,
}
impl ReferenceGraph {
    pub fn new() -> Self {
        ReferenceGraph {
            refnodes: HashMap::new()
        }
    }

    /// Trace the reference graph from the node with `name and mark all nodes
    /// as used. `name` is the name of the method, without leading "parse".
    pub fn trace(&mut self, name: Rc<String>) {
        // The set of edges to trace in the current iteration.
        let mut edges: HashSet<Rc<String>> = HashSet::new();
        edges.insert(name);

        // Iterate over the remaining edges until all edges are traced.
        loop {
            // The set of edges to trace in the next iteration.
            let mut next_edges: HashSet<Rc<String>> = HashSet::new();

            for edge in edges {
                let refnode = self.refnodes.get(&edge)
                    .unwrap_or_else(|| panic!("While computing dependencies, node {} doesn't exist", edge));
                if *refnode.used.borrow() {
                    continue;
                }

                refnode.used.replace(true);

                for next_edge in refnode.edges.iter() {
                    next_edges.insert(next_edge.clone());
                }
            }

            if next_edges.len() == 0 {
                break;
            }

            edges = next_edges;
        }
    }

    /// Return true if the method named `name` (without leading "parse") is
    /// used.
    pub fn is_used(&self, name: Rc<String>) -> bool {
        match self.refnodes.get(&name) {
            Some(refnode) => *refnode.used.borrow(),
            None => false
        }
    }

    /// Insert a node with `name` to the graph.
    pub fn insert(&mut self, name: Rc<String>, edges: HashSet<Rc<String>>) {
        self.refnodes.insert(name, Rc::new(ReferenceGraphNode::new(edges)));
    }
}
