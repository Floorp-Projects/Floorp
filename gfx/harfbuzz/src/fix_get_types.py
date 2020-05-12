import re
import argparse

if __name__=='__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('input')
    parser.add_argument('output')
    args = parser.parse_args()

    with open(args.input, 'r') as inp:
        with open(args.output, 'w') as out:
            for l in inp.readlines():
                l = re.sub('_t_get_type', '_get_type', l)
                l = re.sub('_T \(', ' (', l)
                out.write(l)
