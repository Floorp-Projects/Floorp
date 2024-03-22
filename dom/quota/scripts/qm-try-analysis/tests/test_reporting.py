import copy

from qm_try_analysis.report import remove_duplicates


class MockClient(object):
    def __init__(self, search_results, remote_bugs):
        self.search_results = copy.deepcopy(search_results)
        self.remote_bugs = copy.deepcopy(remote_bugs)

    def get_bug(self, bug_id):
        for item in self.search_results:
            if bug_id == item["id"]:
                return copy.deepcopy(item)
        for item in self.remote_bugs:
            if bug_id == item["id"]:
                return copy.deepcopy(item)
        return {"id": bug_id, "dupe_of": None}


def test_duplicate_bug_removal():
    test_cases = [
        {
            "search_results": [
                {"id": "k1", "dupe_of": "k4"},
                {"id": "k3", "dupe_of": "k5"},
                {"id": "k2", "dupe_of": "k6"},
            ],
            "remote_bugs": [
                {"id": "k4", "dupe_of": "k2"},
                {"id": "k5", "dupe_of": None},
                {"id": "k6", "dupe_of": "k3"},
            ],
            "expected": [{"id": "k5", "dupe_of": None}],
        },
        {
            "search_results": [
                {"id": "k2", "dupe_of": "k6"},
                {"id": "k3", "dupe_of": "k5"},
                {"id": "k1", "dupe_of": "k4"},
            ],
            "remote_bugs": [
                {"id": "k4", "dupe_of": "k2"},
                {"id": "k5", "dupe_of": None},
                {"id": "k6", "dupe_of": "k3"},
            ],
            "expected": [{"id": "k5", "dupe_of": None}],
        },
        {
            "search_results": [
                {"id": "k1", "dupe_of": "k3"},
                {"id": "k2", "dupe_of": "k4"},
            ],
            "remote_bugs": [
                {"id": "k3", "dupe_of": "k2"},
                {"id": "k4", "dupe_of": None},
            ],
            "expected": [{"id": "k4", "dupe_of": None}],
        },
        {
            "search_results": [
                {"id": "k1", "dupe_of": "k3"},
                {"id": "k2", "dupe_of": None},
            ],
            "remote_bugs": [
                {"id": "k3", "dupe_of": "k4"},
                {"id": "k4", "dupe_of": "k2"},
            ],
            "expected": [{"id": "k2", "dupe_of": None}],
        },
        {
            "search_results": [
                {"id": "k1", "dupe_of": "k3"},
                {"id": "k2", "dupe_of": None},
            ],
            "remote_bugs": [{"id": "k3", "dupe_of": "k2"}],
            "expected": [{"id": "k2", "dupe_of": None}],
        },
        {
            "search_results": [
                {"id": "k1", "dupe_of": None},
                {"id": "k2", "dupe_of": "k3"},
            ],
            "remote_bugs": [{"id": "k3", "dupe_of": "k1"}],
            "expected": [{"id": "k1", "dupe_of": None}],
        },
    ]

    for test_case in test_cases:
        search_results = test_case["search_results"]
        remote_bugs = test_case["remote_bugs"]
        expected = test_case["expected"]

        mock_client = MockClient(search_results, remote_bugs)

        assert remove_duplicates(search_results, mock_client) == expected
