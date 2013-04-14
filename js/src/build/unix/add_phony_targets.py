import pymake.data
import pymake.parser
import pymake.parserdata
import sys

'''
Modifies the output of Sun Studio's -xM to look more like the output
of gcc's -MD -MP, adding phony targets for dependencies.
'''


def add_phony_targets(path):
    print path
    deps = set()
    targets = set()
    for stmt in pymake.parser.parsefile(path):
        if isinstance(stmt, pymake.parserdata.Rule):
            assert isinstance(stmt.depexp, pymake.data.StringExpansion)
            assert isinstance(stmt.targetexp, pymake.data.StringExpansion)
            for d in stmt.depexp.s.split():
                deps.add(d)
            for t in stmt.targetexp.s.split():
                targets.add(t)
    phony_targets = deps - targets
    if not phony_targets:
        return
    with open(path, 'a') as f:
        f.writelines('%s:\n' % d for d in phony_targets)


if __name__ == '__main__':
    for f in sys.argv[1:]:
        add_phony_targets(f)
