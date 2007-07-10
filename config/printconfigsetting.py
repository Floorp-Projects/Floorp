import configobj, sys

try:
    (file, section, key) = sys.argv[1:]
except ValueError:
    print "Usage: printconfigsetting.py <file> <section> <setting>"
    sys.exit(1)

c = configobj.ConfigObj(file)

try:
    s = c[section]
except KeyError:
    print >>sys.stderr, "Section [%s] not found." % section
    sys.exit(1)

try:
    print s[key]
except KeyError:
    print >>sys.stderr, "Key %s not found." % key
    sys.exit(1)
