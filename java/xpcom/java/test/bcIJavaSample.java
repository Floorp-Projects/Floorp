/**
 * Interface nsISample
 *
 * IID: 0xca1e2656-1dd1-11b2-9c4e-f49ea557abde
 */

import org.mozilla.xpcom.*;

public interface bcIJavaSample 
{
    public static final String BC_IJAVASAMPLE_IID_STRING =
        "ca1e2656-1dd1-11b2-9c4e-f49ea557abde";
    void queryInterface(IID iid, Object[] result);
    void test0();
    void test1(int l);
    void test2(bcIJavaSample o);
    void test3(int count, int[] valueArray);
    void test4(int count, String[][] valueArray);
}
