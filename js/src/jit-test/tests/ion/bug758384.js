function TEST(section, expected, actual) {}
function TEST_XML(section, expected, actual) {
        return TEST(section, expected, actual.toXMLString());
}
x = new XML("");
TEST_XML(2, "", x);
TEST_XML(3, "", x);
