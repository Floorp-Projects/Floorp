namespace js {

ForkJoinSlice *
ForkJoinSlice::current() {
#ifdef JS_THREADSAFE_ION
    return (ForkJoinSlice*) PR_GetThreadPrivate(ThreadPrivateIndex);
#else
    return NULL;
#endif
}

}
