#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

using namespace std;

struct Arena {
	const char *name;

	Arena(const char *name): name(name) {}
};


void *operator new(size_t size, Arena &arena);
void operator delete(void *p);

void *operator new(size_t size, Arena &arena)
{
	void *p = malloc(size);
	printf("Allocating %d bytes at %p using arena \"%s\"\n", size, p, arena.name);
	return p;
}

void operator delete(void *p)
{
	printf("Deleting object at %p\n", p);
}


struct C {
	int n;

	C(int n, bool bad = false);
	~C();
};

struct Exception {
	int num;

	explicit Exception(int n): num(n) {}
};


C::C(int n, bool bad): n(n)
{
	printf("Constructing C #%d at %p\n", n, this);
	if (bad) {
		printf("Throwing %d; constructor aborted\n", n);
		throw Exception(n);
	}
}

C::~C()
{
	printf("Destroying C #%d at %p\n", n, this);
}


static void constructorTest1(int n, bool bad)
{
	try {
		printf("Calling C(%d,%d)\n", n, (int)bad);
		{
			C c(n, bad);
			printf("We have C #%d\n", c.n);
		}
		printf("C is out of scope\n");
	} catch (Exception &e) {
		printf("Caught exception %d\n", e.num);
	}
	printf("\n");
}


static void constructorTest2(int n, bool bad)
{
	try {
		printf("Calling new C(%d,%d)\n", n, (int)bad);
		{
			C *c = new C(n, bad);
			printf("We have C #%d\n", c->n);
			delete c;
		}
		printf("C is out of scope\n");
	} catch (Exception &e) {
		printf("Caught exception %d\n", e.num);
	}
	printf("\n");
}


static void constructorTest3(int n, bool bad)
{
	try {
		printf("Calling new(arena) C(%d,%d)\n", n, (int)bad);
		{
			Arena arena("My arena");
			C *c = new(arena) C(n, bad);
			printf("We have C #%d\n", c->n);
		}
		printf("C is out of scope\n");
	} catch (Exception &e) {
		printf("Caught exception %d\n", e.num);
	}
	printf("\n");
}


int main()
{
	printf("Beginning constructor tests\n\n");
	constructorTest1(1, false);
	constructorTest1(2, true);
	constructorTest2(3, false);
	constructorTest2(4, true);
	constructorTest3(5, false);
	constructorTest3(6, true);
	printf("Ending constructor tests\n");
	return 0;
}
