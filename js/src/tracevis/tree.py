#!/usr/bin/python
# vim: set ts=2 sw=2 tw=99 noet: 
import re
import sys
import string

class Node:
	def __init__(self, address):
		self.address = address

class Fragment:
	def __init__(self, address, root, pc, file, line, offset):
		self.address = 'x' + address[2:]
		if root == None:
			self.root = self
		else:
			self.root = root
		self.pc = pc
		self.file = file
		self.line = line
		self.exits = []
		self.marked = False
		self.offset = offset
		self.stackTypes = None
		self.globalTypes = None
		self.lastExit = None
		self.nodes = {}

	def addNode(self, exit):
		node = Node('L' + self.address + 'E' + exit.address)
		self.nodes[exit.address] = node
		return node

	def nodeName(self, exit):
		return self.nodes[exit.address].address

	def type(self):
		if self == self.root:
			return 'loop'
		else:
			return 'branch'

	def describe(self):
		if self.stackTypes == None:
			return '<' + self.type() + '<br/>' + self.line + '@' + self.offset + \
						 '<br/>' + self.address + '>'
		else:
			return '<' + self.type() + '<br/>' + self.line + '@' + self.offset + \
						 '<br/>' + self.address + ', ' + self.stackTypes + ',' + self.globalTypes + '>'

	def addExit(self, e):
		e.owner = self
		self.exits.append(e)

class Exit:
	def __init__(self, address, type, pc, file, line, offs, _s, _g):
		self.address = 'x' + address[2:]
		self.type = type
		self.pc = pc
		self.file = file
		self.line = line
		self.offset = offs
		self.stackTypes = _s
		self.globalTypes = _g
		self.owner = None
		self.target = None
		self.treeGuard = None
		self.marked = False

	def describe(self):
		return '<' + self.type + '<br/>' + self.address + ', line ' + self.line + '@' + self.offset + \
					 '<br/>' + self.stackTypes + ',' + self.globalTypes + '>'

class TraceRecorder:
	def __init__(self):
		self.trees = {}
		self.fragments = {}
		self.exits = {}
		self.currentFrag = None
		self.currentAnchor = None
		self.edges = []
		self.treeEdges = []

	def createTree(self, ROOT, PC, FILE, LINE, STACK, GLOBALS, OFFS):
		f = Fragment(ROOT, None, PC, FILE, LINE, OFFS)
		f.stackTypes = STACK
		f.globalTypes = GLOBALS
		self.trees[ROOT] = f
		self.fragments[ROOT] = f
		return f

	def createBranch(self, ROOT, FRAG, PC, FILE, LINE, ANCHOR, OFFS):
		root = self.trees[ROOT]
		branch = Fragment(FRAG, root, PC, FILE, LINE, OFFS)
		self.fragments[FRAG] = branch
		return branch

	def record(self, FRAG, ANCHOR):
		self.currentFrag = self.fragments[FRAG]
		if self.currentFrag.root != self.currentFrag:
			self.currentAnchor = self.exits[ANCHOR]
		else:
			self.currentAnchor = None

	def addExit(self, EXIT, TYPE, FRAG, PC, FILE, LINE, OFFS, STACK, GLOBALS):
		if EXIT not in self.exits:
			e = Exit(EXIT, TYPE, PC, FILE, LINE, OFFS, STACK, GLOBALS)
			e.owner = self.fragments[FRAG]
			self.exits[EXIT] = e
		else:
			e = self.exits[EXIT]
		self.currentFrag.addExit(e)

	def changeExit(self, EXIT, TYPE):
		exit = self.exits[EXIT]
		exit.type = TYPE

	def endLoop(self, EXIT):
		exit = self.exits[EXIT]
		if self.currentAnchor != None:
			self.currentAnchor.target = self.currentFrag
		self.currentFrag.lastExit = exit
		self.currentFrag = None
		self.currentAnchor = None

	def closeLoop(self, EXIT, PEER):
		exit = self.exits[EXIT]
		if self.currentAnchor != None:
			self.currentAnchor.target = self.currentFrag
		if exit.type != "LOOP" and PEER != "0x0":
			peer = self.trees[PEER]
			exit.target = peer
		self.currentFrag.lastExit = exit
		self.currentFrag = None
		self.currentAnchor = None

	def join(self, ANCHOR, FRAG):
		exit = self.exits[ANCHOR]
		fragment = self.fragments[FRAG]
		exit.target = fragment

	def treeCall(self, INNER, EXIT, GUARD):
		exit = self.exits[EXIT]
		inner = self.trees[INNER]
		guard = self.exits[GUARD]
		exit.target = inner
		exit.treeGuard = guard

	def trash(self, FRAG):
		pass

	def readLine(self, line):
		cmd = re.match(r"TREEVIS ([^ ]+) (.*)$", line)
		if cmd == None:
			return
		cmd = cmd.groups()
		keys = dict([w.split('=') for w in string.split(cmd[1])])
		if cmd[0] == "CREATETREE":
			self.createTree(**keys)
		elif cmd[0] == "RECORD":
			self.record(**keys)
		elif cmd[0] == "ADDEXIT":
			self.addExit(**keys)
		elif cmd[0] == "CREATEBRANCH":
			self.createBranch(**keys)
		elif cmd[0] == "CLOSELOOP":
			self.closeLoop(**keys)
		elif cmd[0] == "CHANGEEXIT":
			self.changeExit(**keys)
		elif cmd[0] == "ENDLOOP":
			self.endLoop(**keys)
		elif cmd[0] == "JOIN":
			self.join(**keys)
		elif cmd[0] == "TRASH":
			self.trash(**keys)
		elif cmd[0] == "TREECALL":
			self.treeCall(**keys)
		else:
			raise Exception("unknown command: " + cmd[0])

	def describeTree(self, fragment):
		return '<' + 'tree call to<br/>' + fragment.line + '@' + fragment.offset + '>'

	def emitExit(self, fragment, exit):
		if exit.type == "NESTED":
			print '\t\ttc_' + exit.address + ' [ shape = octagon, label = ' + \
						self.describeTree(exit.target) + ' ]'
			self.edges.append(['tc_' + exit.address, exit.target.address])
		node = fragment.addNode(exit)
		print '\t\t' + node.address + ' [ label = ' + exit.describe() + ' ]'

	def emitTrace(self, fragment):
		if fragment.marked == True:
			raise Exception('fragment already marked')
		print "\t\t" + fragment.address + " [ label = <start> ] "
		for e in fragment.exits:
			if e.target == None and (e != fragment.lastExit and e.type != 'LOOP'):
				continue
			self.emitExit(fragment, e)
		last = fragment.address
		for e in fragment.exits:
			if e.target == None and (e != fragment.lastExit and e.type != 'LOOP'):
				continue
			if e.type == "NESTED":
				print '\t\t' + last + ' -> tc_' + e.address
				print '\t\ttc_' + e.address + ' -> ' + fragment.nodeName(e) + \
							' [ arrowtail = dot, arrowhead = onormal ] '
				if e.treeGuard != None:
					self.treeEdges.append([e, fragment.nodeName(e)])
			else:
				print '\t\t' + last + ' -> ' + fragment.nodeName(e)
				if e.target == None and e.type == 'LOOP':
					self.edges.append([fragment.nodeName(e), fragment.root.address])
				elif e.target != None:
					self.edges.append([fragment.nodeName(e), e.target.address])
			last = fragment.nodeName(e)

	def emitBranch(self, fragment):
		if fragment.root == fragment:
			raise Exception('branches have root!=frag')
		if fragment.lastExit == None:
			fragment.marked = True
			return
		print '\tsubgraph cluster_' + fragment.address + ' {'
		print '\t\tlabel = ' + fragment.describe()
		self.emitTrace(fragment)
		fragment.marked = True
		print "\t}"
		self.emitBranches(fragment)

	def emitBranches(self, fragment):
		for e in fragment.exits:
			if not e.target or e.target.marked:
				continue
			if e.target == e.target.root:
				self.emitTree(e.target)
			else:
				self.emitBranch(e.target)

	def emitTree(self, fragment):
		if fragment.root != fragment:
			raise Exception('trees have root=frag')
		if fragment.lastExit == None:
			fragment.marked = True
			return
		print '\tsubgraph cluster_' + fragment.address + ' {'
		print '\t\tlabel = ' + fragment.describe()
		self.emitTrace(fragment)
		print "\t}"
		fragment.marked = True
		self.emitBranches(fragment)

	def emitGraph(self):
		print "digraph G {"
		worklist = [self.trees[fragment] for fragment in self.trees]
		for fragment in worklist:
			if fragment.marked:
				continue
			self.emitTree(fragment)
		for edge in self.edges:
			name = edge[0]
			address = edge[1]
			print '\t' + name + ' -> ' + address
		for edge in self.treeEdges:
			fromName = edge[0].treeGuard.owner.nodeName(edge[0].treeGuard)
			toName = edge[1]
			print '\t' + fromName + ' -> ' + toName + \
					  ' [ arrowtail = dot ]'
		print "}"

	def fromFile(self, file):
		line = file.readline()
		while line != "":
			self.readLine(line)
			line = file.readline()

tr = TraceRecorder()
f = open(sys.argv[1], "r")
tr.fromFile(f)
f.close()
tr.emitGraph()

