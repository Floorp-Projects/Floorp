/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  // Blah! First source!
}

test.prototype = {
  anonymousExpression: function () {
  },
  namedExpression: function NAME() {
  },
  sub: {
    sub: {
      sub: {
      }
    }
  }
};

var foo = {
  a_test: function () {
  },
  n_test: function x() {
  },
  sub: {
    a_test: function () {
    },
    n_test: function y() {
    },
    sub: {
      a_test: function () {
      },
      n_test: function z() {
      },
      sub: {
        test_SAME_NAME: function test_SAME_NAME() {
        }
      }
    }
  }
};
