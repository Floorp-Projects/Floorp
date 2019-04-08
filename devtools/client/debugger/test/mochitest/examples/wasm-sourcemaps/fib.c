/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

void swap(int* a, int* b) {
  int t = *a;
  *a = *b;
  *b = t;
}

int fib(int n) {
  int i, t, a = 0, b = 1;
  for (i = 0; i < n; i++) {
    a += b;
    swap(&a, &b);
  }
  return b;
}
