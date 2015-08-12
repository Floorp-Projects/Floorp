// |jit-test| exitstatus: 3

if (typeof 'oomAtAllocation' === 'undefined')
    quit(3);

function fn(i) {
    if (i == 3)
      return ["isFinite"].map(function (i) {});
    return [];
}

fn(0);
fn(1);
fn(2);
oomAtAllocation(50);
fn(3);

quit(3);
