#include "test.h"
#include <stdio.h>

class testimpl : public test
{
public:
    testimpl() : mIntProp(-5) { }
    NS_DECL_ISUPPORTS
    NS_DECL_TEST
private:
    PRInt32 mIntProp;
};

NS_IMPL_ISUPPORTS1(testimpl, test);

NS_IMETHODIMP
testimpl::Poke(const char *with)
{
    fprintf(stderr, "poke: %s!\n", with);
    return NS_OK;
}

NS_IMETHODIMP
testimpl::Say(const char *sayIt)
{
    fprintf(stderr, "testimpl says: %s!\n", sayIt);
    return NS_OK;
}

NS_IMETHODIMP
testimpl::Shout(const char *shoutIt)
{
    fprintf(stderr, "testimpl shouts: %s!\n", shoutIt);
    return NS_OK;
}

NS_IMETHODIMP
testimpl::Add(PRInt32 a, PRInt32 b, PRInt32 *result)
{
    *result = a + b;
    fprintf(stderr, "%d(%08x) + %d(%08x) = %d(%08x)\n", a, a, b, b,
            *result, *result);
    return NS_OK;
}

NS_IMETHODIMP
testimpl::Peek(char **retval)
{
    *retval = strdup("AHOY!");
    fprintf(stderr, "ahoy is %p @ %p\n", *retval, retval);
    return NS_OK;
}

NS_IMETHODIMP
testimpl::Callback(testCallback *cb)
{
    fprintf(stderr, "testCallback is %p\n", cb);
    return cb->Call();
}

NS_IMETHODIMP
testimpl::GetIntProp(PRInt32 *aIntProp)
{
    *aIntProp = mIntProp;
    return NS_OK;
}

NS_IMETHODIMP
testimpl::SetIntProp(PRInt32 aIntProp)
{
    mIntProp = aIntProp;
    return NS_OK;
}

NS_IMETHODIMP
testimpl::GetRoIntProp(PRInt32 *aRoIntProp)
{
    *aRoIntProp = 42;
    return NS_OK;
}

extern "C" testimpl*
GetImpl(void)
{
    return new testimpl;
}
