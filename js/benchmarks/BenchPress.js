//********* BenchPress-v3.js **********
//
//  A set of benchmarks to measure the efficiency of language implementations.
//  Based in part on the Stanford Integer Benchmarks originally written by
//  John Hennesey, modified by Peter Nye, and further modified by members of
//  the Self group under David Ungar and Randy Smith.
//
//  To be consistent with other languages, some benchmarks use 1-based indexing.
//  To support this, arrays are created with an extra element and the contents
//  of their zeroth entries are ignored.
//
//
// John Maloney, August 1995
//
//***************************************

/********** AbstractBenchmark **********/


function randInit()
{
    this.seed = 74755;
}

function rand()
{
  this.seed = (( this.seed * 1309) + 13849) & 0xffff;
  return this.seed;
}

function testRand() {
  this.randInit();
  if ((this.rand() != 22896) ||
    (this.rand() != 34761) ||
    (this.rand() != 34014) ||
    (this.rand() != 39231) ||
    (this.rand() != 52540) ||
    (this.rand() != 41445) ||
    (this.rand() !=  1546) ||
    (this.rand() !=  5947) ||
    (this.rand() != 65224)) {
      this.error("Random number generator");
  }
}

function unimplemented() {
  print("<not yet implemented>");
}


function AbstractBenchmark_error(s)
{
    print("Error in " + s);
}

function AbstractBenchmark()
{
    this.error = AbstractBenchmark_error;
    this.rand = rand;
    this.randInit = randInit;
    this.run = unimplemented;
    this.seed = 0;
}
AbstractBenchmark.Vect = new Array(10000);

/********** AbstractSortBenchmark **********/

function checkArray(a) {
  var lastIndex, i;

  /* Note: sort benchmarks use 1-based indexing */
  lastIndex = a.length - 1;
  if ((a[1] != this.littlest) || (a[lastIndex] != this.biggest)) {
    this.error("Sort Benchmark: Array is not sorted");
    return;
  }
  for (i = 2; i <= lastIndex; i++) {
    if (a[i - 1] > a[i]) {
      this.error("Sort Benchmark: Array is not sorted");
      return;
    }
  }
}

function randomArray(arraySize) {
  var a;
  var i, val;
  this.randInit();
  a = new Array(arraySize + 1);  /* extra for 1-based indexing */
  this.biggest = -10000;
  this.littlest = 10000;
  for (i = 1; i <= arraySize; i++) {
    val = this.rand();
    a[i] = val;
    if (val > this.biggest) this.biggest = val;
    if (val < this.littlest) this.littlest = val;
  }
  return a;
}

function AbstractSortBenchmark()
{
    this.biggest = 0;
    this.littlest = 0;
    this.randomArray = randomArray;
    this.checkArray = checkArray;
}
AbstractSortBenchmark.prototype = new AbstractBenchmark;


/********** SieveBenchmark **********/

function sieve(flags, flagsSize)
{
  var primeCount = 0;

  for (var i = 0; i <= flagsSize; i++) {
    flags[i] = true;
  }
  for (i = 2; i <= flagsSize; i++) {
    if (flags[i]) {
      primeCount++;
      for (var k = i + i; k <= flagsSize; k = k + i) {
        flags[k] = false; /* k is not prime */
      }
    }
  }
  return primeCount;
}

function SieveBenchmark_run()
{
    if (sieve(this.flags, 8190) != 1027)
        this.error("SieveBenchmark");
}

function SieveBenchmark()
{
    this.flags = new Array(8191);
    this.run = SieveBenchmark_run;
}
SieveBenchmark.prototype = new AbstractBenchmark;

/********** BounceBenchmark **********/

function Ball()
{
    this.x = 0;
    this.y = 0;
    this.xVel = 0;
    this.yVel = 0;
    this.initializeFor = initializeFor;
    this.bounce = bounce;
}

function initializeFor(bm)
{
  this.x = bm.rand() % 500;
    this.y = bm.rand() % 500;
    this.xVel = (bm.rand() % 300) - 150;
  this.yVel = (bm.rand() % 300) - 150;
  return this;
}

function bounce()
{
  var xLimit = 500;
  var yLimit = 500;
  var bounced = false;

  this.x = this.x + this.xVel;
  this.y = this.y + this.yVel;
  if (this.x > xLimit) {
    this.x = xLimit;
    this.xVel = 0 - Math.abs(this.xVel);
    bounced = true;
  }
  if (this.x < 0) {
    this.x = 0;
    this.xVel = Math.abs(this.xVel);
    bounced = true;
  }
  if (this.y > yLimit) {
    this.y = yLimit;
    this.yVel = 0 - Math.abs(this.yVel);
    bounced = true;
  }
  if (this.y < 0) {
    this.y = 0;
    this.yVel = Math.abs(this.yVel);
    bounced = true;
  }
  return bounced;
}

function BounceBenchmark()
{
    this.run = BounceBenchmark_run;
}
BounceBenchmark.prototype = new AbstractBenchmark;

function BounceBenchmark_run()
{
  var ballCount = 100;
  var balls;
  var bounces = 0;
  var i, j;
  this.randInit();
  balls = new Array(ballCount);
  for (i = 0; i < ballCount; i++) {
    balls[i] = (new Ball()).initializeFor(this);
  }
  for (i = 0; i < 10; i++) {
    for (j = 0; j < ballCount; j++) {
      if (balls[j].bounce()) bounces++;
    }
  }
  if (bounces != 268) this.error("BounceBenchmark " + bounces);
}

/********** NestedForLoopBenchmark **********/

function NestedForLoopBenchmark_run()
{
  var sum, i, j;

  sum = 0;
  for (i = 1000; i > 0; i--) {
    for (j = 100; j > 0; j--) {
      sum = sum + 1;
    }
  }
  if (sum != 100000) {
    this.error("NestedForLoopBenchmark");
  }
}

function NestedForLoopBenchmark()
{
    this.run = NestedForLoopBenchmark_run;
}
NestedForLoopBenchmark.prototype = new AbstractBenchmark;

/********** NestedWhileLoopBenchmark **********/

function NestedWhileLoopBenchmark_run()
{
  var sum, i, j;

  sum = 0;
  i = 1000;
  while (i > 0) {
    j = 100;
    while (j > 0) {
      sum = sum + 1;
      j--;
    }
    i--;
  }
  if (sum != 100000) {
    this.error("NestedWhileLoopBenchmark");
  }
}

function NestedWhileLoopBenchmark()
{
    this.run = NestedWhileLoopBenchmark_run;
}
NestedWhileLoopBenchmark.prototype = new AbstractBenchmark;

/********** PermBenchmark **********/

function PermBenchmark()
{
    this.count    = 0;
    this.v        = 0;
    this.run      = PermBenchmark_run;
    this.permute  = permute;
    this.swap     = swap;
}
PermBenchmark.prototype = new AbstractBenchmark;

function swap(i, j)
{
    var tmp         = this.v[i];
    this.v[i]       = this.v[j];
    this.v[j]       = tmp;
}

function permute(n)
{
    var i;

  this.count++;
  if (n != 1) {
    this.permute(n - 1);
    for (i = n - 1; i > 0; i--) {
      this.swap(n, i);
      this.permute(n - 1);
      this.swap(n, i);
    }
  }
}

function PermBenchmark_run()
{
  this.count = 0;
  this.v = new Array(null,null,null,null,null,null,null,null);  /* extra for 1-based indexing */
  this.permute(7);
  if (this.count != 8660) {
    this.error("PermBenchmark");
  }
}

/********** QueensBenchmark **********/

function QueensBenchmark()
{
  this.freeRows = null;
  this.freeMajs = null;
  this.freeMins = null;
  this.qrows = null;
  this.run = QueensBenchmark_run;
  this.placeQueen = placeQueen;
  this.doQueens = doQueens;
}
QueensBenchmark.prototype = new AbstractBenchmark;

function placeQueen(col) {
  for (var row = 1; row <= 8; row++) {
    if (this.freeRows[row] && this.freeMajs[(col + row) - 1]
                  && this.freeMins[(col - row) + 7]) {
      this.qrows[col] = row;
      this.freeRows[row] = false;
      this.freeMajs[(col + row) - 1] = false;
      this.freeMins[(col - row) + 7] = false;
      if (col == 8) return true;
      if (this.placeQueen(col + 1)) return true;
      this.freeRows[row] = true;
      this.freeMajs[(col + row) - 1] = true;
      this.freeMins[(col - row) + 7] = true;
    }
  }
  return false;
}

function doQueens()
{
  var i;

  /* allocate extra element in each array for 1-based indexing */
  this.freeRows = new Array(9);
  for (i = 8; i > 0; i--) {
    this.freeRows[i] = true;
  }
  this.freeMajs = new Array(16);
  for (i = 15; i > 0; i--) {
    this.freeMajs[i] = true;
  }
  this.freeMins = new Array(16);
  for (i = 15; i > 0; i--) {
    this.freeMins[i] = true;
  }
  this.qrows = new Array(9);
  for (i = 8; i > 0; i--) {
    this.qrows[i] = -1;
  }
  if (!this.placeQueen(1)) this.error("QueensBenchmark");
}

function QueensBenchmark_run()
{
  for (var i = 10; i > 0; i--)
      this.doQueens();
}

/********** QuicksortBenchmark **********/

function QuicksortBenchmark()
{
    this.run = QuicksortBenchmark_run;
    this.sort = sort;
}
QuicksortBenchmark.prototype = new AbstractSortBenchmark;

function sort(a, l, r)
{
  var i, j, pivot, temp;

  i = l;
  j = r;
  pivot = a[Math.floor((l + r) / 2)];
  while (i <= j) {
    while (a[i] < pivot) i++;
    while (pivot < a[j]) j--;
    if (i <= j) {
      temp = a[i];
      a[i] = a[j];
      a[j] = temp;
      i++;
      j--;
    }
  }
  if (l < j) this.sort(a, l, j);
  if (i < r) this.sort(a, i, r);
}

function QuicksortBenchmark_run()
{
  var v;
  v = this.randomArray(1000);
  this.sort(v, 1, 1000);
  this.checkArray(v);
}


/********** RecurseBenchmark **********/

function RecurseBenchmark()
{
    this.run = RecurseBenchmark_run;
    this.recurse = recurse;
}
RecurseBenchmark.prototype = new AbstractBenchmark;

function recurse(n)
{
  if (n > 0) {
    this.recurse(n - 1);
    this.recurse(n - 1);
  }
}

function RecurseBenchmark_run()
{
  this.recurse(14);
}

/********** BubbleSortBenchmark **********/

function bubbleSort(a)
{
  var top, i, curr, next;

  top = a.length - 1;
  while (top > 1) {
    i = 1;
    while (i < top) {
      curr = a[i];
      next = a[i + 1];
      if (curr > next) {
        a[i] = next;
        a[i + 1] = curr;
      }
      i++;
    }
    top--;
  }
}

function BubbleSortBenchmark()
{
    this.run = BubbleSortBenchmark_run;
    this.bubbleSort = bubbleSort;
}
BubbleSortBenchmark.prototype = new AbstractSortBenchmark;

function BubbleSortBenchmark_run()
{
  var a;
  a = this.randomArray(250);
  this.bubbleSort(a);
  this.checkArray(a);
}


/********** IncrementAllBenchmark **********/

function IncrementAllBenchmark()
{
    this.run = IncrementAllBenchmark_run;
}
IncrementAllBenchmark.prototype = new AbstractBenchmark;

function IncrementAllBenchmark_run() {
  var oldVal, i;

  oldVal = AbstractBenchmark.Vect[1];
  for (i = AbstractBenchmark.Vect.length - 1; i >= 0; i--) {
    AbstractBenchmark.Vect[i] = AbstractBenchmark.Vect[i] + 1;
  }
  if (AbstractBenchmark.Vect[1] != (oldVal + 1)) {
    this.error("IncrementAllBenchmark");
  }
}

/********** MMIntBenchmark **********/

function IntegerMatrix(rowCount, columnCount)
{
  this.rows = rowCount;
  this.columns = columnCount;
  this.data = new Array(rowCount);
  for (var i = rowCount - 1; i >= 0; i--) {
    this.data[i] = new Array(columnCount);
  }
  this.fillFor = IntegerMatrix_fillFor;
  this.dotProduct = IntegerMatrix_dotProduct;
  this.multiplyBy = IntegerMatrix_multiplyBy;
}

function IntegerMatrix_fillFor(bm)
{
  for (var i = this.rows - 1; i >= 0; i--) {
    for (var j = this.columns - 1; j >= 0; j--) {
      this.data[i][j] = (bm.rand() % 120) - 60;
    }
  }
}

function IntegerMatrix_dotProduct(row, rowSize, otherM, columnIndex)
{
  var sum = 0;
  for (var i = 0; i < rowSize; i++) {
    sum = sum + (row[i] * otherM.data[i][columnIndex]);
  }
  return sum;
}

function IntegerMatrix_multiplyBy(m)
{
  var rowCount = this.rows;
  var columnCount = m.columns;
  var result = new IntegerMatrix(rowCount, columnCount);
  for (var i = 0; i < rowCount; i++) {
    for (var j = 0; j < columnCount; j++) {
      result.data[i][j] =
        this.dotProduct(this.data[i], this.columns, m, j);
    }
  }
  return result;
}

function MMIntBenchmark()
{
    this.run = MMIntBenchmark_run;
}
MMIntBenchmark.prototype = new AbstractBenchmark;

function MMIntBenchmark_run()
{
  var ma, mb, mr;

  ma = new IntegerMatrix(10, 10);
  mb = new IntegerMatrix(10, 10);
  ma.fillFor(this);
  mb.fillFor(this);
  mr = ma.multiplyBy(mb);
}

/********** MMFloatBenchmark **********/

function FloatMatrix(rowCount, columnCount)
{
  this.rows = rowCount;
  this.columns = columnCount;
  this.data = new Array(rowCount);
  for (var i = rowCount - 1; i >= 0; i--) {
    this.data[i] = new Array(columnCount);
  }
  this.fillFor = FloatMatrix_fillFor;
  this.dotProduct = FloatMatrix_dotProduct;
  this.multiplyBy = FloatMatrix_multiplyBy;
}

function FloatMatrix_fillFor(bm)
{
  for (var i = this.rows - 1; i >= 0; i--) {
    for (var j = this.columns - 1; j >= 0; j--) {
      this.data[i][j] = (1.0 * ((bm.rand() % 120) - 60));
    }
  }
}

function FloatMatrix_dotProduct(row, rowSize, otherM, columnIndex)
{
  var sum = 0;
  for (var i = 0; i < rowSize; i++) {
    sum = sum + (row[i] * otherM.data[i][columnIndex]);
  }
  return sum;
}

function FloatMatrix_multiplyBy(m) {
  var rowCount = this.rows;
  var columnCount = m.columns;
  var result = new FloatMatrix(rowCount, columnCount);
  for (var i = 0; i < rowCount; i++) {
    for (var j = 0; j < columnCount; j++) {
      result.data[i][j] =
        this.dotProduct(this.data[i], this.columns, m, j);
    }
  }
  return result;
}

function MMFloatBenchmark()
{
    this.run = MMFloatBenchmark_run;
}
MMFloatBenchmark.prototype = new AbstractBenchmark;

function MMFloatBenchmark_run()
{
  var ma, mb, mr;

  ma = new FloatMatrix(10, 10);
  mb = new FloatMatrix(10, 10);
  ma.fillFor(this);
  mb.fillFor(this);
  mr = ma.multiplyBy(mb);
}

/********** AtAllPutBenchmark **********/

function AtAllPutBenchmark()
{
    this.run = AtAllPutBenchmark_run;
}
AtAllPutBenchmark.prototype = new AbstractBenchmark;

function AtAllPutBenchmark_run()
{
  var i;
  for (i = AbstractBenchmark.Vect.length - 1; i >= 0; i--) {
    AbstractBenchmark.Vect[i] = 5;
  }
  if (AbstractBenchmark.Vect[1] != 5)
      this.error("AtAllPutBenchmark");
}

/********** StorageBenchmark **********/

function StorageBenchmark()
{
    this.count = 0;
    this.run = StorageBenchmark_run;
    this.buildTreeDepth = buildTreeDepth;
}
StorageBenchmark.prototype = new AbstractBenchmark;

function buildTreeDepth(depth)
{
  this.count++;
  if (depth == 1) {
    return new Array(Math.floor((this.rand() % 10) + 1));
  } else {
    var newNode  = new Array(4);
    newNode[0]   = this.buildTreeDepth(depth - 1);
    newNode[1]   = this.buildTreeDepth(depth - 1);
    newNode[2]   = this.buildTreeDepth(depth - 1);
    newNode[3]   = this.buildTreeDepth(depth - 1);
    return newNode;
  }
}

function StorageBenchmark_run()
{
  this.randInit();
  this.count = 0;
  this.buildTreeDepth(6);
  if (this.count != 1365) {
    this.error("StorageBenchmark");
  }
}

/********** SumAllBenchmark **********/

function SumAllBenchmark()
{
    this.run = SumAllBenchmark_run;
}
SumAllBenchmark.prototype = new AbstractBenchmark;

function SumAllBenchmark_run()
{
  var elementValue, sum, i;

  elementValue = AbstractBenchmark.Vect[1];
  sum = 0;
  for (i = AbstractBenchmark.Vect.length - 1; i >= 0; i--) {
    sum = sum + AbstractBenchmark.Vect[i];
  }
  if (sum != (AbstractBenchmark.Vect.length * elementValue)) {
    this.error("SumAllBenchmark");
  }
}

/********** SumFromToBenchmark **********/

function SumFromToBenchmark()
{
    this.run = SumFromToBenchmark_run;
    this.sumFromTo = sumFromTo;
}
SumFromToBenchmark.prototype = new AbstractBenchmark;

function sumFromTo(start, end)
{
  var sum = 0;
  for (var i = start; i <= end; i++) {
    sum = sum + i;
  }
  return sum;
}

function SumFromToBenchmark_run() {
  for (var i = 9; i > 0; i--)
      this.sumFromTo(1, 10000);
  if (this.sumFromTo(1, 10000) != 50005000) {
    this.error("SumFromToBenchmark");
  }
}


/********** TakBenchmark **********/

function TakBenchmark()
{
    this.run = TakBenchmark_run;
    this.tak = tak;
}
TakBenchmark.prototype = new AbstractBenchmark;

function tak(x, y, z)
{
  if (y < x) {
    return this.tak(
      this.tak(x - 1, y, z),
      this.tak(y - 1, z, x),
      this.tak(z - 1, x, y));
  } else {
    return z;
  }
}

function TakBenchmark_run()
{
  var r = this.tak(18, 12, 6);
  if (r != 7) this.error("TakBenchmark");
}

/********** TaklBenchmark **********/

function ListElement(n)
{
  this.val = n;
  this.next = null;
}

function listn(n)
{
  if (n == 0) {
    return null;
  } else {
    var newEl = new ListElement(n);
    newEl.next = listn(n - 1);
    return newEl;
  }
}

function takl_length(l)
{
  if (l.next == null) {
    return 1;
  } else {
    return 1 + takl_length(l.next);
  }
}

function shorterp(x, y)
{
  var xTail, yTail;

  xTail = x;
  yTail = y;
  while (yTail != null) {
    if (xTail == null) return true;
    xTail = xTail.next;
    yTail = yTail.next;
  }
  return false;
}

function takl(x, y, z)
{
  if (shorterp(y, x)) {
    return takl(
      takl(x.next, y, z),
      takl(y.next, z, x),
      takl(z.next, x, y));
  } else {
    return z;
  }
}

function TaklBenchmark_run()
{
  var r = takl(listn(18), listn(12), listn(6));
  if (takl_length(r) != 7) {
    this.error("TaklBenchmark");
  }
}

function TaklBenchmark()
{
    this.run = TaklBenchmark_run;
}
TaklBenchmark.prototype = new AbstractBenchmark;


/********** TowersBenchmark **********/

function TowersDisk(size)
{
  this.diskSize = size;
  this.next = null;
}

function TowersBenchmark()
{
    this.piles = new Array(4);
    this.movesdone = 0;
    this.push = push;
    this.pop = pop;
    this.moveTopDisk = moveTopDisk;
    this.buildTowerAt = buildTowerAt;
    this.emptyPileAt = emptyPileAt;
    this.tower = tower;
    this.run = TowersBenchmark_run;
}
TowersBenchmark.prototype = new AbstractBenchmark;

function push(d, pileIndex)
{
  var top = this.piles[pileIndex];
  if ((top != null) && (d.diskSize >= top.diskSize)) {
    this.error("TowersBenchmark: cannot put a big disk on a smaller one");
  }
  d.next = top;
  this.piles[pileIndex] = d;
}

function pop(pileIndex)
{
  var top = this.piles[pileIndex];
  if (top == null) {
    this.error("TowersBenchmark: attempting to remove a disk from an empty pile");
   }
  this.piles[pileIndex] = top.next;
  top.next = null;
  return top;
}

function moveTopDisk(fromPileIndex, toPileIndex)
{
  this.push(this.pop(fromPileIndex), toPileIndex);
  this.movesdone++;
}

function buildTowerAt(pileIndex, levels)
{
  for (var i = levels; i > 0; i--) {
    this.push(new TowersDisk(i), pileIndex);
  }
}

function emptyPileAt(pileIndex)
{
  this.piles[pileIndex] = null;
}

function tower(i, j, k)
{
  if (k == 1) {
    this.moveTopDisk(i, j);
  } else {
    var other = (6 - i) - j;
    this.tower(i, other, k - 1);
    this.moveTopDisk(i, j);
    this.tower(other, j, k - 1);
  }
}

function TowersBenchmark_run()
{
  this.emptyPileAt(1);
  this.emptyPileAt(2);
  this.emptyPileAt(3);
  this.buildTowerAt(1, 14);
  this.movesdone = 0;
  this.tower(1, 2, 14);
  if (this.movesdone != 16383) this.error("TowersBenchmark");
}

/********** TreeSortBenchmark **********/

function TreeNode(n)
{
  this.left = null;
  this.right = null;
  this.val = n;
    this.checkTree = checkTree;
    this.insert = insert;
}

function checkTree()
{
  return ((this.left == null) ||
                ((this.left.val < this.val) && this.left.checkTree())) &&
              ((this.right == null) ||
                ((this.right.val >= this.val) && this.right.checkTree()));
}

function insert(n)
{
  if (n < this.val) {
    if (this.left == null) {
      this.left = new TreeNode(n);
    } else {
      this.left.insert(n);
    }
  } else {
    if (this.right == null) {
      this.right = new TreeNode(n);
    } else {
      this.right.insert(n);
    }
  }
}

function TreeSortBenchmark()
{
    this.run = TreeSortBenchmark_run;
}
TreeSortBenchmark.prototype = new AbstractSortBenchmark;

function TreeSortBenchmark_run()
{
  var vSize = 1000;

  var v = this.randomArray(vSize);
  var tree = new TreeNode(v[1]);
  for (var i = 2; i <= vSize; i++) {
    tree.insert(v[i]);
  }
  if (!tree.checkTree()) {
    this.error("TreeSortBenchmark");
  }
}

/********** BenchmarkRunner **********/

function print2col(col1, col2) {
    while (col1.length < 30)
  col1 += " ";
    print(col1 + col2);
}

function timeit(name, benchmark)
{
    benchmark.run();
    var startTime = new Date();
    benchmark.run();
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print2col(name, elapsedTime);
    return elapsedTime;
}


function BenchmarkRunner()
{
    var total = 0;

    print("Benchpress(v3) in JavaScript...");
    print2col("benchmark", "time (ms)");
    total = total + timeit("AtAllPutBenchmark",     new AtAllPutBenchmark());
    total += timeit("BounceBenchmark",              new BounceBenchmark());
    total += timeit("BubbleSortBenchmark",          new BubbleSortBenchmark());
    total += timeit("IncrementAllBenchmark",        new IncrementAllBenchmark());
    total += timeit("MMIntBenchmark",               new MMIntBenchmark());
    total += timeit("MMFloatBenchmark",             new MMFloatBenchmark());
    total += timeit("NestedForLoopBenchmark",       new NestedForLoopBenchmark());
    total += timeit("NestedWhileLoopBenchmark",     new NestedWhileLoopBenchmark());
    total += timeit("PermBenchmark",                new PermBenchmark());
    total += timeit("QueensBenchmark",              new QueensBenchmark());
    total += timeit("QuicksortBenchmark",           new QuicksortBenchmark());
    total += timeit("RecurseBenchmark",             new RecurseBenchmark());
    total += timeit("SieveBenchmark",               new SieveBenchmark());
    total += timeit("StorageBenchmark",             new StorageBenchmark());
    total += timeit("SumAllBenchmark",              new SumAllBenchmark());
    total += timeit("SumFromToBenchmark",           new SumFromToBenchmark());
    total += timeit("TakBenchmark",                 new TakBenchmark());
    total += timeit("TaklBenchmark",                new TaklBenchmark());
    total += timeit("TowersBenchmark",              new TowersBenchmark());
    total += timeit("TreeSortBenchmark",            new TreeSortBenchmark());

    print2col("total", total);
}


BenchmarkRunner();
