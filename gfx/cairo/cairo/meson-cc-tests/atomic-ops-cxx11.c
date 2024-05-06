int atomic_add(int i) { return __atomic_fetch_add(&i, 1, __ATOMIC_SEQ_CST); }
int atomic_cmpxchg(int i, int j, int k) { return __atomic_compare_exchange_n(&i, &j, k, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }
int main(void) { return 0; }
