__attribute__((__warn_unused_result__)) void f (void) {}
__attribute__((__warn_unused_result__)) int g;

int main(int c, char **v)
{
    (void)c;
    (void)v;
    return 0;
}
